// RpgRules - "idle life" for WORLD bots, the port of Karrz's ike3 rpg strategy
// (RpgAction / ChooseRpgTargetAction / RpgSubActions, the "city" named travel
// target and the AH travel purpose) onto the snapshot -> rules -> intents model.
//
//   idle:rpg_visit     A bot in a town walks from NPC to NPC: it picks a nearby
//                      service NPC (vendor, trainer, innkeeper, banker,
//                      auctioneer, flight/stable master, battlemaster) weighted
//                      by how useful the visit is and how far it is (ike3's
//                      relevance / (1 + distance / 30)), walks up, faces it,
//                      talks (gossip hello + an emote), sells its grey junk and
//                      repairs if the NPC offers it (ike3 rpg sell / rpg
//                      repair), lingers a few seconds and moves on. Visited
//                      NPCs are remembered so it does not ping-pong (ike3's
//                      "ignore rpg target"). Arriving next to an auctioneer,
//                      banker, trainer, flight master or innkeeper also lets the
//                      existing service rules (ah_*, bank_deposit, trainer,
//                      taxi_discover, homebind) do their work, so this rule only
//                      decides WHERE to go and how to behave while there.
//   idle:rpg_city_trip A questless bot in its rpg phase now and then travels to
//                      the nearest city with an auction house, bank or
//                      battlemaster and hangs around there for a long town
//                      session (ike3 "should travel named::city": rpg phase
//                      <= 20 of a 60-minute cycle, 10% chance per pick).
//   idle:rpg_ah_trip   A bot with BoE gear worth listing travels to the nearest
//                      auctioneer its side may use (ike3 AH travel purpose) and
//                      lets idle:ah_post_surplus list it. Classic auctioneers
//                      are in the faction capitals and the goblin towns (Booty
//                      Bay, Gadgetzan, Everlook); the house an auctioneer serves
//                      follows its faction (AuctionHouseMgr), so any auctioneer
//                      the bot may walk up to is a valid destination.
//
// Every rule here is a world errand (prefix "idle:rpg_" in IsWorldErrand): a
// companion in its owner's group never runs them. They also require the bot to
// be ungrouped, out of instances, and never preempt a quest action in reach
// (has_actionable_quest). Town presence and destinations come from
// TownNpcIndex (world spawn data, vanilla service flags only).

#include "Bot/IdleRule.h"
#include "Group/GroupSnapshot.h"
#include "Bot/BotAI.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotIntent.h"
#include "Bot/ClassicUpkeep.h"
#include "Services.h"
#include "Util/ConfigReader.h"
#include "Bot/States/MaintainHelpers.h"   // ChunkedWalkToward, DriveTravelPlanTo
#include "Travel/RegionMapper.h"
#include "Travel/TownNpcIndex.h"
#include "ItemTemplate.h"
#include "ObjectMgr.h"
#include "SharedDefines.h"
#include "UnitDefines.h"

#include <cmath>
#include <vector>

namespace Playerbot {

namespace {

using ::Playerbot::V2::Travel::TownNpc;
using ::Playerbot::V2::Travel::TownNpcIndex;

// NPCs that make a place a town, and that a strolling bot visits.
constexpr uint32 kTownFlags =
      UNIT_NPC_FLAG_TRAINER | UNIT_NPC_FLAG_VENDOR_MASK | UNIT_NPC_FLAG_REPAIR
    | UNIT_NPC_FLAG_FLIGHTMASTER | UNIT_NPC_FLAG_INNKEEPER | UNIT_NPC_FLAG_BANKER
    | UNIT_NPC_FLAG_PETITIONER | UNIT_NPC_FLAG_TABARDDESIGNER | UNIT_NPC_FLAG_BATTLEMASTER
    | UNIT_NPC_FLAG_AUCTIONEER | UNIT_NPC_FLAG_STABLEMASTER;
// ike3 "city": the NPCs only a city has.
constexpr uint32 kCityFlags = UNIT_NPC_FLAG_AUCTIONEER | UNIT_NPC_FLAG_BANKER | UNIT_NPC_FLAG_BATTLEMASTER;

constexpr float  kTownRadius      = 60.0f;   // NPCs counted / visited around the bot
constexpr uint32 kTownMinNpcs     = 3;       // a lone roadside vendor is not a town
constexpr float  kTalkDist        = 3.5f;    // stand this far from the NPC
constexpr float  kInnFarDist      = 7.0f;    // innkeeper we must not bind at
constexpr float  kArriveTrip      = 40.0f;
constexpr float  kAhTripMaxDist   = 1500.0f;
constexpr float  kCityTripMaxDist = 3000.0f;
constexpr uint32 kApproachTimeout = 40u * 1000u;
constexpr uint32 kStaleVisitMs    = 120u * 1000u;
constexpr uint32 kTripTimeout     = 20u * 60u * 1000u;
constexpr uint8  kAhPostMinLevel  = 25;      // idle:ah_post_surplus lists from this level
constexpr uint32 kMin             = 60u * 1000u;

float Dist2d(float ax, float ay, float bx, float by)
{
    const float dx = ax - bx, dy = ay - by;
    return std::sqrt(dx * dx + dy * dy);
}

// Karrz's ike3 RPG_PHASE_NUMBER: a per-bot number that walks 0..60 once a
// minute; the bot is in its "rpg phase" (free to idle in towns or go to a city
// even with quests open) while it reads <= 20, about a third of every hour,
// with each bot's window at a different time.
bool InRpgPhase(BotSnapshotView const& s, uint32 now_ms)
{
    const uint32 seed = uint32(s.bot_id()) * 2654435761u;
    return ((seed % 61u) + now_ms / kMin) % 61u <= 20u;
}

bool Questless(BotSnapshotView const& s)
{
    return s.current_quest_id() == 0 && !s.has_relocation_target();
}

// May this bot spend time on idle life at all right now?
bool MayIdle(BotSnapshotView const& s, uint32 now_ms)
{
    return Questless(s) || InRpgPhase(s, now_ms);
}

// The world-bot preconditions every rpg rule shares.
bool WorldBotFree(BotSnapshotView const& s, BotAI& ai, uint32 now_ms)
{
    if (!s.is_alive() || s.in_combat() || s.is_casting()) return false;
    if (s.is_stunned() || s.is_rooted()) return false;
    if (s.in_group()) return false;                       // a group moves together
    if (s.is_in_instance() || s.is_in_dungeon() || s.in_battleground()) return false;
    if (s.on_transport()) return false;
    if (ai.in_profession_mode(now_ms)) return false;
    if (s.has_actionable_quest()) return false;           // quest work in reach always wins
    if (Classic::RecoverBeforeWalking(s)) return false;   // hurt, mid-meal or dry: recover first
    return true;
}

bool InTown(BotSnapshotView const& s)
{
    float bx, by, bz; s.position(bx, by, bz);
    return TownNpcIndex::Instance().CountNear(s.map_id(), bx, by, kTownRadius, s.is_alliance(), kTownFlags)
        >= kTownMinNpcs;
}

bool HasGreyJunk(BotSnapshotView const& s)
{
    for (auto const& it : s.raw().inventory.bag_items)
        if (!it.guid.IsEmpty() && !it.is_quest_item && it.quality == ITEM_QUALITY_POOR)
            return true;
    return false;
}

// BoE gear idle:ah_post_surplus can list: one non-stackable uncommon..epic item
// that is not bind-on-pickup, not conjured and not already soulbound.
uint32 AhListableCount(BotSnapshotView const& s)
{
    // Bot auctions are off by default (PlayerbotV2.Economy.BotAuctions): nothing is listable, so no AH errands.
    if (!Services::Config().economy_bot_auctions_enabled()) return 0;
    uint32 n = 0;
    for (auto const& it : s.raw().inventory.bag_items)
    {
        if (it.guid.IsEmpty() || it.is_quest_item || it.count != 1) continue;
        if (it.quality < ITEM_QUALITY_UNCOMMON || it.quality > ITEM_QUALITY_EPIC) continue;
        if (it.stats.is_soulbound) continue;
        ItemTemplate const* tmpl = sObjectMgr->GetItemTemplate(it.entry);
        if (!tmpl || tmpl->GetMaxStackSize() != 1) continue;
        if (tmpl->GetBonding() == BIND_ON_ACQUIRE || tmpl->GetBonding() == BIND_QUEST) continue;
        if (tmpl->HasFlag(ITEM_FLAG_CONJURED)) continue;
        ++n;
    }
    return n;
}

// How worth visiting is this NPC right now (ike3 rpg trigger relevances,
// normalised: sell 100, repair 95, ah sell 110, train 80, trivial emote 1).
float Relevance(BotSnapshotView const& s, TownNpc const& n, bool grey, bool damaged, bool listable)
{
    float r = 1.0f;
    auto best = [&](float v) { if (v > r) r = v; };
    if ((n.npc_flags & UNIT_NPC_FLAG_VENDOR_MASK) && grey)        best(100.0f);
    if ((n.npc_flags & UNIT_NPC_FLAG_REPAIR) && damaged)          best(95.0f);
    if ((n.npc_flags & UNIT_NPC_FLAG_AUCTIONEER) && listable && s.level() >= kAhPostMinLevel) best(110.0f);
    if ((n.npc_flags & UNIT_NPC_FLAG_TRAINER) && n.trainer_class != 0 && n.trainer_class == s.cls()) best(60.0f);
    if (n.npc_flags & UNIT_NPC_FLAG_INNKEEPER)                    best(25.0f);
    if (n.npc_flags & UNIT_NPC_FLAG_BANKER)                       best(15.0f);
    if ((n.npc_flags & UNIT_NPC_FLAG_BATTLEMASTER) && s.level() >= 10) best(15.0f);
    if ((n.npc_flags & UNIT_NPC_FLAG_STABLEMASTER) && s.cls() == CLASS_HUNTER) best(20.0f);
    if (n.npc_flags & UNIT_NPC_FLAG_AUCTIONEER)                   best(10.0f);
    if (n.npc_flags & UNIT_NPC_FLAG_VENDOR_MASK)                  best(10.0f);
    if (n.npc_flags & UNIT_NPC_FLAG_TRAINER)                      best(6.0f);
    if (n.npc_flags & UNIT_NPC_FLAG_FLIGHTMASTER)                 best(5.0f);
    return r;
}

// The live creature of a visit target: same entry, standing near its spawn.
NearbyUnit const* LiveNpc(BotSnapshotView const& s, BotAI::RpgState const& st)
{
    NearbyUnit const* best = nullptr;
    float best_d = 20.0f;
    for (auto const& u : s.raw().combat.nearby_friends)
    {
        if (u.is_player || u.entry != st.entry) continue;
        const float d = Dist2d(u.x, u.y, st.x, st.y);
        if (d < best_d) { best_d = d; best = &u; }
    }
    return best;
}

// Stop short of an innkeeper when the hearth is already bound near it, so
// idle:homebind (5y) only rebinds when the bind is far away (ike3 rpg home
// bind: only when the current bind is >500y off).
float StopDistance(BotSnapshotView const& s, BotAI::RpgState const& st)
{
    if (!(st.npc_flags & UNIT_NPC_FLAG_INNKEEPER))
        return kTalkDist;
    if (s.homebind_map_id() == s.map_id() && Dist2d(s.homebind_x(), s.homebind_y(), st.x, st.y) < 500.0f)
        return kInnFarDist;
    return kTalkDist;
}

void StartSession(BotSnapshotView const& s, BotAI& ai, uint32 now_ms, uint8 visits, uint32 duration_ms)
{
    auto& st = ai.rpg();
    st.visits_left    = visits;
    st.session_end_ms = now_ms + duration_ms;
    (void)s;
}

void EndSession(BotSnapshotView const& s, BotAI& ai, uint32 now_ms)
{
    auto& st = ai.rpg();
    st.visits_left = 0;
    st.session_end_ms = 0;
    // Questless bots live in town; questing ones only drop in between quests.
    st.next_session_ms = now_ms + (Questless(s) ? uint32(ai.rng().int_range(3, 7)) * kMin
                                                : uint32(ai.rng().int_range(10, 21)) * kMin);
}

void BeginVisit(BotSnapshotView const& s, BotAI& ai, TownNpc const& n, uint32 now_ms)
{
    auto& st = ai.rpg();
    st.spawn_id       = n.spawn_id;
    st.entry          = n.entry;
    st.npc_flags      = n.npc_flags;
    st.x = n.x; st.y = n.y; st.z = n.z;
    st.npc.Clear();
    st.phase          = 1;
    st.phase_start_ms = now_ms;
    st.last_move_ms   = 0;
    st.blocked_base   = s.path_blocked_count();
}

// Choose the next NPC of the town (ike3 ChooseRpgTargetAction).
bool PickTarget(BotSnapshotView const& s, BotAI& ai, uint32 now_ms)
{
    float bx, by, bz; s.position(bx, by, bz);
    const bool grey     = HasGreyJunk(s);
    const bool damaged  = s.lowest_equipped_durability_pct() < 100 && s.gold() > 0;
    const bool listable = AhListableCount(s) > 0;
    auto const& st = ai.rpg();

    struct Cand { TownNpc const* n; float w; };
    std::vector<Cand> cands;
    float total = 0.f;
    bool any_useful = false;
    TownNpcIndex::Instance().ForEachNear(s.map_id(), bx, by, kTownRadius, [&](TownNpc const& n)
    {
        if (!(n.npc_flags & kTownFlags) || !n.usable_by(s.is_alliance())) return;
        if (st.visited_lately(n.spawn_id, now_ms)) return;
        if (std::fabs(n.z - bz) > 25.0f) return;   // another floor / ledge: not a stroll
        const float rel = Relevance(s, n, grey, damaged, listable);
        if (rel > 1.0f) any_useful = true;
        const float w = rel / (1.0f + Dist2d(bx, by, n.x, n.y) / 30.0f);
        cands.push_back({ &n, w });
    });
    if (cands.empty())
        return false;
    // ike3: once a useful target exists, trivial (emote-only) ones are dropped.
    if (any_useful)
        std::erase_if(cands, [&](Cand const& c) { return Relevance(s, *c.n, grey, damaged, listable) <= 1.0f; });
    for (Cand const& c : cands) total += c.w;
    double roll = ai.rng().unit() * total;
    TownNpc const* pick = cands.back().n;
    for (Cand const& c : cands)
    {
        roll -= c.w;
        if (roll <= 0.0) { pick = c.n; break; }
    }
    BeginVisit(s, ai, *pick, now_ms);
    return true;
}

// Talk to the NPC now that the bot stands next to it (ike3 rpg emote / sell /
// repair sub-actions).
bool Arrive(BotSnapshotView const& s, BotAI& ai, BotIntentEmitter& emit, NearbyUnit const* live,
            float dist, uint32 now_ms)
{
    auto& st = ai.rpg();
    if (s.is_mounted())
        emit.dismount();

    bool sit = false;
    if (live)
    {
        st.npc = live->guid;
        emit.face_target(live->guid);
        const uint64 low = live->guid.GetCounter();
        const bool in_reach = dist <= 5.0f;
        if (in_reach && (live->npc_flags & UNIT_NPC_FLAG_GOSSIP))
            emit.interact_with_npc(live->guid);
        if (in_reach && (live->npc_flags & UNIT_NPC_FLAG_VENDOR_MASK) && HasGreyJunk(s)
            && !ai.action_recently_tried(BotAI::ActionKind::SellTrash, low, now_ms))
        {
            emit.vendor_sell_trash(live->guid);
            ai.note_action_retry(BotAI::ActionKind::SellTrash, low, now_ms);
        }
        if (in_reach && (live->npc_flags & UNIT_NPC_FLAG_REPAIR) && s.lowest_equipped_durability_pct() < 100
            && s.gold() > 0 && !ai.action_recently_tried(BotAI::ActionKind::Repair, low, now_ms))
        {
            emit.repair_all(live->guid);
            ai.note_action_retry(BotAI::ActionKind::Repair, low, now_ms);
        }
        // A rest at the inn now and then (ike3 rpg stay).
        sit = (st.npc_flags & UNIT_NPC_FLAG_INNKEEPER) && ai.rng().chance_pct(35);
    }
    if (sit)
        emit.sit();
    else
    {
        // Talk, bow, wave, exclaim, ask, laugh, point: all vanilla one-shot emotes.
        static constexpr uint32 kTalkEmotes[] = {
            EMOTE_ONESHOT_TALK, EMOTE_ONESHOT_TALK, EMOTE_ONESHOT_BOW, EMOTE_ONESHOT_WAVE,
            EMOTE_ONESHOT_EXCLAMATION, EMOTE_ONESHOT_QUESTION, EMOTE_ONESHOT_LAUGH, EMOTE_ONESHOT_POINT,
        };
        const uint32 e = kTalkEmotes[ai.rng().int_range(0, int32(std::size(kTalkEmotes)))];
        emit.perform_emote(e, live ? live->guid : ObjectGuid::Empty);
    }
    st.phase           = 2;
    st.phase_start_ms  = now_ms;
    st.linger_until_ms = now_ms + uint32(ai.rng().int_range(8, 21)) * 1000u;
    ai.set_last_rule_fired("idle:rpg_talk");
    return true;
}

// Walk up to the visit target; talk on arrival. Returns true when the tick is
// consumed.
bool Approach(BotSnapshotView const& s, BotAI& ai, BotIntentEmitter& emit, uint32 now_ms)
{
    auto& st = ai.rpg();
    float bx, by, bz; s.position(bx, by, bz);
    NearbyUnit const* live = LiveNpc(s, st);
    float tx = st.x, ty = st.y, tz = st.z;
    if (live) { tx = live->x; ty = live->y; tz = live->z; }
    const float stop = StopDistance(s, st);
    const float d = Dist2d(bx, by, tx, ty);

    if (d <= stop + 1.0f && std::fabs(tz - bz) < 6.0f)
        return Arrive(s, ai, emit, live, d, now_ms);

    // Unreachable, gone, or taking too long: forget it for a while, try another.
    const bool blocked = s.path_blocked_count() > st.blocked_base;
    const bool vanished = !live && d < 12.0f;
    if (blocked || vanished || now_ms - st.phase_start_ms > kApproachTimeout)
    {
        st.remember(st.spawn_id, now_ms);
        st.end_visit();
        return false;
    }
    if (st.last_move_ms != 0 && now_ms - st.last_move_ms < 2000u)
        return true;   // a move was just issued; let it start

    st.last_move_ms = now_ms;
    if (d > 60.0f)
        return States::ChunkedWalkToward(s, ai, emit, tx, ty, tz, "idle:rpg_visit", now_ms);
    // Stand at talking distance on the bot's side of the NPC.
    const float k = (d - stop) / d;
    emit.move_to(bx + (tx - bx) * k, by + (ty - by) * k, tz, /*run*/ d > 20.0f);
    ai.set_last_rule_fired("idle:rpg_visit");
    return true;
}

// ---------- idle:rpg_visit ----------
bool VisitGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32 now_ms)
{
    if (!WorldBotFree(s, ai, now_ms)) return false;
    auto const& st = ai.rpg();
    // A live trip owns the bot's movement; one abandoned long ago (quest work
    // took over) no longer does.
    if (st.trip != 0 && now_ms - st.trip_start_ms <= kTripTimeout) return false;
    if (st.phase == 2) return true;
    if (st.phase == 1) return !s.is_moving();
    if (s.is_moving()) return false;
    const bool session = st.visits_left > 0 && now_ms < st.session_end_ms;
    if (!session)
    {
        if (now_ms < st.next_session_ms) return false;
        if (!MayIdle(s, now_ms)) return false;
    }
    return InTown(s);
}

bool VisitFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter& emit, uint32 now_ms)
{
    auto& st = ai.rpg();
    if (st.trip != 0)
        st.end_trip();   // stale, see VisitGate
    // A visit interrupted by combat, a quest or travel is not resumed later.
    if (st.phase != 0 && now_ms - st.phase_start_ms > kStaleVisitMs)
        st.end_visit();

    if (st.phase == 2)
    {
        float bx, by, bz; s.position(bx, by, bz);
        const bool walked_off = Dist2d(bx, by, st.x, st.y) > 15.0f;
        if (now_ms < st.linger_until_ms && !walked_off)
        {
            ai.set_last_rule_fired("idle:rpg_linger");
            return true;
        }
        st.remember(st.spawn_id, now_ms);
        st.end_visit();
        if (st.visits_left > 0) --st.visits_left;
        if (st.visits_left == 0 || now_ms >= st.session_end_ms)
        {
            EndSession(s, ai, now_ms);
            return false;
        }
        // Straight on to the next NPC (ike3 rpg cancel -> choose rpg target).
    }

    if (st.phase == 0)
    {
        const bool session = st.visits_left > 0 && now_ms < st.session_end_ms;
        if (!session)
        {
            if (Questless(s))
                StartSession(s, ai, now_ms, uint8(ai.rng().int_range(3, 8)), 15u * kMin);
            else
                StartSession(s, ai, now_ms, uint8(ai.rng().int_range(1, 4)), 6u * kMin);
        }
        if (!PickTarget(s, ai, now_ms))
        {
            EndSession(s, ai, now_ms);
            return false;
        }
    }
    return Approach(s, ai, emit, now_ms);
}

// ---------- trips ----------
enum : uint8 { kTripNone = 0, kTripCity = 1, kTripAh = 2 };

void BeginTrip(BotSnapshotView const& s, BotAI& ai, uint8 kind, TownNpc const& dest, uint32 now_ms)
{
    auto& st = ai.rpg();
    st.end_visit();
    st.trip          = kind;
    st.trip_map      = dest.map_id;
    st.trip_x = dest.x; st.trip_y = dest.y; st.trip_z = dest.z;
    st.trip_start_ms = now_ms;
    st.last_mount_ms = 0;
    (void)s;
}

// Drive the bot toward the trip destination with the shared travel executors
// (travel graph for far / cross-landmass legs, chunked walk otherwise).
// Returns true when it emitted; false when the trip ended (arrived or gave up),
// with `arrived` telling which.
bool DriveTrip(BotSnapshotView const& s, BotAI& ai, BotIntentEmitter& emit, char const* tag,
               uint32 now_ms, bool& arrived)
{
    auto& st = ai.rpg();
    arrived = false;
    if (s.map_id() != st.trip_map || now_ms - st.trip_start_ms > kTripTimeout)
    {
        st.end_trip();
        return false;
    }
    float bx, by, bz; s.position(bx, by, bz);
    const float d = Dist2d(bx, by, st.trip_x, st.trip_y);
    if (d <= kArriveTrip)
    {
        arrived = true;
        st.end_trip();
        return false;
    }
    if (!s.is_mounted() && !s.is_indoors() && d > 150.0f
        && (st.last_mount_ms == 0 || now_ms - st.last_mount_ms > 30000u))
    {
        st.last_mount_ms = now_ms;
        if (emit.mount_appropriate())
        {
            ai.set_last_rule_fired(tag);
            return true;
        }
    }
    using ::Playerbot::V2::Travel::RegionForPosition;
    const bool same_land = RegionForPosition(s.map_id(), bx, by) == RegionForPosition(st.trip_map, st.trip_x, st.trip_y);
    if (!same_land || d > 250.0f)
        if (States::DriveTravelPlanTo(s, ai, emit, bx, by, bz, st.trip_map, st.trip_x, st.trip_y, st.trip_z,
                                      /*quest_goal=*/false))
            return true;
    if (same_land && States::ChunkedWalkToward(s, ai, emit, st.trip_x, st.trip_y, st.trip_z, tag, now_ms))
        return true;
    // No way there from here this time.
    st.end_trip();
    return false;
}

// ---------- idle:rpg_city_trip ----------
bool CityTripGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32 now_ms)
{
    if (!WorldBotFree(s, ai, now_ms)) return false;
    auto const& st = ai.rpg();
    if (st.trip == kTripCity) return !s.is_moving();
    if (st.trip != kTripNone) return false;
    if (s.level() <= 5 || !Questless(s) || !InRpgPhase(s, now_ms)) return false;
    if (now_ms < st.next_city_trip_ms || now_ms < st.next_city_roll_ms) return false;
    if (st.visits_left > 0 && now_ms < st.session_end_ms) return false;   // busy in this town
    if (s.is_moving()) return false;
    float bx, by, bz; s.position(bx, by, bz);
    // Already in a city: nothing to travel to.
    return TownNpcIndex::Instance().CountNear(s.map_id(), bx, by, 150.0f, s.is_alliance(), kCityFlags) == 0;
}

bool CityTripFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter& emit, uint32 now_ms)
{
    auto& st = ai.rpg();
    if (st.trip == kTripNone)
    {
        st.next_city_roll_ms = now_ms + 3u * kMin;
        if (!ai.rng().chance_pct(25))
            return false;
        float bx, by, bz; s.position(bx, by, bz);
        TownNpcIndex const& idx = TownNpcIndex::Instance();
        TownNpc const* dest = idx.Nearest(s.map_id(), bx, by, bz, kCityFlags, s.is_alliance(), kCityTripMaxDist);
        if (!dest)
        {
            st.next_city_trip_ms = now_ms + 30u * kMin;
            return false;
        }
        BeginTrip(s, ai, kTripCity, *dest, now_ms);
        st.next_city_trip_ms = now_ms + 45u * kMin;
    }
    bool arrived = false;
    if (DriveTrip(s, ai, emit, "idle:rpg_city_trip", now_ms, arrived))
        return true;
    if (arrived)
    {
        // Hang around in the city: a long town session starts now.
        StartSession(s, ai, now_ms, uint8(ai.rng().int_range(4, 9)), 20u * kMin);
        st.next_session_ms = 0;
    }
    return false;
}

// ---------- idle:rpg_ah_trip ----------
bool AhTripGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32 now_ms)
{
    if (!WorldBotFree(s, ai, now_ms)) return false;
    auto const& st = ai.rpg();
    if (st.trip == kTripAh) return !s.is_moving();
    if (st.trip != kTripNone) return false;
    if (s.level() < kAhPostMinLevel || now_ms < st.next_ah_trip_ms) return false;
    if (!MayIdle(s, now_ms) || s.is_moving()) return false;
    if (s.gold() < 10000) return false;                    // listing deposits
    return AhListableCount(s) >= 2;
}

bool AhTripFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter& emit, uint32 now_ms)
{
    auto& st = ai.rpg();
    if (st.trip == kTripNone)
    {
        st.next_ah_trip_ms = now_ms + 45u * kMin;
        float bx, by, bz; s.position(bx, by, bz);
        TownNpc const* ah = TownNpcIndex::Instance().Nearest(s.map_id(), bx, by, bz, UNIT_NPC_FLAG_AUCTIONEER,
                                                              s.is_alliance(), kAhTripMaxDist);
        if (!ah)
            return false;
        BeginTrip(s, ai, kTripAh, *ah, now_ms);
        // Remember which auctioneer, so the last steps walk up to that NPC.
        st.spawn_id = ah->spawn_id; st.entry = ah->entry; st.npc_flags = ah->npc_flags;
        st.x = ah->x; st.y = ah->y; st.z = ah->z;
    }
    bool arrived = false;
    if (DriveTrip(s, ai, emit, "idle:rpg_ah_trip", now_ms, arrived))
        return true;
    if (!arrived || st.spawn_id == 0)
        return false;
    // The last few yards are a normal visit to the auctioneer; the ah_* rules
    // list and buy once the bot stands at the auctioneer.
    if (st.visits_left == 0 || now_ms >= st.session_end_ms)
        StartSession(s, ai, now_ms, uint8(ai.rng().int_range(2, 5)), 10u * kMin);
    st.phase          = 1;
    st.phase_start_ms = now_ms;
    st.last_move_ms   = 0;
    st.blocked_base   = s.path_blocked_count();
    return Approach(s, ai, emit, now_ms);
}

} // anonymous namespace

void RegisterRpgRules(IdleRuleRegistry& r)
{
    // Built here, on the world thread at module init, before any bot worker ticks.
    TownNpcIndex::Instance().Initialize();

    // All three sit below every need-driven rule (vendor 500, mail 470-480,
    // ah 445-460, bank 360-380, homebind 238-240, trainer 228-230, the quest
    // band 698+) and above the ambient flavour (yell_lfg 100, sit 90, emote 80)
    // and the autoact cascade (50), so a bot only idles when nothing needs doing.
    {
        IdleRule rule;
        rule.name     = "idle:rpg_ah_trip";
        rule.priority = 135;
        rule.gate     = &AhTripGate;
        rule.fire     = &AhTripFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:rpg_city_trip";
        rule.priority = 130;
        rule.gate     = &CityTripGate;
        rule.fire     = &CityTripFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:rpg_visit";
        rule.priority = 110;
        rule.gate     = &VisitGate;
        rule.fire     = &VisitFire;
        r.register_rule(std::move(rule));
    }
}

} // namespace Playerbot
