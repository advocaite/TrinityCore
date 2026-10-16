// BgTeamCoordinator implementation. See header for the design contract.
//
// Planning model
// --------------
// Every kPlanIntervalMs the coordinator rebuilds the full order map from
// scratch (registry walk -> bucket by (BG instance, team) -> per-family
// planner). Hysteresis is applied INSIDE the assignment step: a bot whose
// previous order matches a candidate (same kind, same target within
// kStickyRadius) gets a cost discount, so stable inputs reproduce the
// previous plan and bots don't thrash between equidistant objectives.
//
// The coordinator only orders V2 bots. Human teammates are observed
// indirectly (through the node pressure counts the builder harvests) but
// never directed. Bots the plan does not cover keep order.kind == None and
// run the legacy greedy role logic — the coordinator concentrates force
// where coordination beats greed and deliberately leaves the rest alone.

#include "BgTeamCoordinator.h"
#include "BgTactics.h"
#include "BattlegroundScript.h"
#include "../BotRegistry.h"
#include "../BotSnapshotView.h"
#include "../ClassTables.h"
#include "../../Services.h"
#include "../../Threading/SnapshotPublisher.h"

#include "Battleground.h"
#include "Config.h"
#include "GameObject.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"

#include "BgAvPlan.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_set>

namespace Playerbot {

namespace {

// Full re-plan cadence. 750ms is fast enough to chase carrier movement and
// node flips (BG state changes on second granularity) while keeping the
// cost negligible: one registry walk + a few hundred float ops per team.
constexpr uint32 kPlanIntervalMs = 750;

// A previous order "matches" a new candidate when same-kind and the target
// moved less than this — used for the hysteresis cost discount.
constexpr float kStickyRadius = 10.0f;

// Cost multiplier for a sticky match. 0.7 means a bot keeps its current
// assignment unless a competing one is >30% closer.
constexpr float kStickyDiscount = 0.7f;

// AV open-field brawl (PlanNodeRace): a human within this many yards of the fight zone counts as fighting
// there; one this far past it toward the enemy base counts as pushing. A follow-mode change must hold for
// kMidModeHoldMs; the brawl stops for good after kMidMaxMs of match time so the game still ends.
constexpr float  kMidHumanRadius = 130.0f;
constexpr uint32 kMidModeHoldMs  = 15u * 1000u;
constexpr uint32 kMidMaxMs       = 40u * 60u * 1000u;

float Dist2(float ax, float ay, float bx, float by)
{
    const float dx = ax - bx, dy = ay - by;
    return dx * dx + dy * dy;
}

bool PosSet(float x, float y) { return x != 0.f || y != 0.f; }

} // namespace

// ---------------------------------------------------------------------------
// Assignment bookkeeping
// ---------------------------------------------------------------------------

void BgTeamCoordinator::AssignOrder(uint64 guid, uint8 kind, float x, float y,
                                    float z, ObjectGuid focus, uint8 squad,
                                    uint32 target_entry)
{
    BgOrder o;
    o.kind         = kind;
    o.x            = x;
    o.y            = y;
    o.z            = z;
    o.focus        = focus;
    o.squad        = squad;
    o.target_entry = target_entry;
    next_orders_[guid] = o;
}

// Pick the cheapest unassigned, alive member for a target, with the
// hysteresis discount against the PREVIOUS order map. `healer_bias` > 0
// pushes healers toward the end of the pick order (defense posts), < 0
// pulls them forward (escort slots), 0 is neutral. For focus-orders
// (escort/hunt) pass the focus guid: stickiness keys on it instead of
// position — the carrier MOVES between plans, so a positional match
// would never hold and escorts would churn every 750ms.
int BgTeamCoordinator::PickNearest(std::vector<Member>& members, uint8 kind,
                                   float tx, float ty, int healer_bias,
                                   bool allow_carrier, ObjectGuid focus) const
{
    int   best     = -1;
    float best_cost = 1e30f;
    for (int i = 0; i < int(members.size()); ++i)
    {
        Member const& m = members[i];
        if (m.assigned || !m.alive) continue;
        if (m.is_carrier && !allow_carrier) continue;
        // Order matters: the sticky discount scales DISTANCE only. Adding
        // the signed healer bias first would let the multiplier magnify
        // (or, on negative costs, invert) the role preference instead of
        // expressing "30% closer before reassignment".
        float cost = std::sqrt(Dist2(m.x, m.y, tx, ty));
        auto prev = orders_.find(m.guid_low);
        if (prev != orders_.end() && prev->second.kind == kind &&
            (!focus.IsEmpty()
                 ? prev->second.focus == focus
                 : Dist2(prev->second.x, prev->second.y, tx, ty) <
                       kStickyRadius * kStickyRadius))
            cost *= kStickyDiscount;
        if (m.healer && healer_bias > 0) cost += 200.f;   // keep healers off lone posts
        if (m.healer && healer_bias < 0) cost -= 150.f;   // prefer healers for escort
        if (cost < best_cost) { best_cost = cost; best = i; }
    }
    return best;
}

// ---------------------------------------------------------------------------
// CTF / carrier family (WSG, TP, Kotmogu, and the flag layer of EotS /
// Deephaul). Assigns: carriers -> CarryHome, a pickup runner when no
// friendly carrier exists, escorts on each carrier, and an EFC hunt squad.
// ---------------------------------------------------------------------------

void BgTeamCoordinator::PlanCtf(TeamPlanContext& ctx)
{
    const bool kotmogu   = ctx.type_id == 699;
    const bool node_play = !ctx.nodes.empty();   // hybrid (EotS/Deephaul)

    // -- Carriers: run it home / hold center --------------------------------
    for (auto& m : ctx.members)
    {
        if (!m.is_carrier || !m.alive) continue;
        float hx = ctx.home_x, hy = ctx.home_y, hz = ctx.home_z;
        if (!kotmogu && node_play)
        {
            // EotS-style: cap at the nearest node we own; if we own none,
            // run at the nearest node at all (capping it wins twice).
            float best = 1e30f;
            bool  found_owned = false;
            for (auto const& n : ctx.nodes)
            {
                if (n.is_destroyed) continue;
                const bool owned = n.owner_team == ctx.team && !n.is_contested;
                if (found_owned && !owned) continue;
                const float d = Dist2(m.x, m.y, n.x, n.y);
                if ((owned && !found_owned) || d < best)
                {
                    best = d; hx = n.x; hy = n.y; hz = n.z;
                    if (owned) found_owned = true;
                }
            }
        }
        else if (!kotmogu && PosSet(ctx.own_flag_x, ctx.own_flag_y))
        {
            // WSG/TP: cap point is the own-flag spawn.
            hx = ctx.own_flag_x; hy = ctx.own_flag_y; hz = ctx.own_flag_z;
        }
        // Kotmogu: home_base IS the center scoring zone — already set.
        if (PosSet(hx, hy))
        {
            AssignOrder(m.guid_low, BgOrder::CarryHome, hx, hy, hz);
            m.assigned = true;
        }
    }

    // -- Dropped flags ---------------------------------------------------------
    // A flag on the ground is the most valuable click on the map: our own one
    // goes home, the enemy one is lifted again before it returns. Send the
    // nearest free bot within reach of every drop (the on-station objective
    // pass clicks it on arrival; the in-combat pass clicks it within 8 yards).
    for (auto const& f : ctx.dropped_flags)
    {
        constexpr float kDropReach = 80.0f;
        int pick = -1;
        float best = kDropReach * kDropReach;
        for (int i = 0; i < int(ctx.members.size()); ++i)
        {
            Member const& m = ctx.members[i];
            if (m.assigned || !m.alive || m.is_carrier) continue;
            const float d = Dist2(m.x, m.y, f.x, f.y);
            if (d < best) { best = d; pick = i; }
        }
        if (pick < 0) continue;
        AssignOrder(ctx.members[pick].guid_low, BgOrder::PickupFlag, f.x, f.y, f.z);
        ctx.members[pick].assigned = true;
    }

    // -- Pickup runner(s) when we have no carrier ---------------------------
    // Kotmogu fields 4 orbs; send up to 2 runners for free ones. Everyone
    // else fields exactly one flag: 1 runner.
    int have_carriers = 0;
    for (auto const& m : ctx.members)
        if (m.is_carrier && m.alive) ++have_carriers;
    // External (human) carriers count toward "we already have the flag"
    // on single-flag maps; on Kotmogu more orbs is always better, so the
    // bot-runner count ignores them there.
    const int want_runners =
        kotmogu ? std::max(0, 2 - have_carriers)
                : ((have_carriers + ctx.external_carriers) > 0 ? 0 : 1);
    std::vector<int> runner_idx;
    if (kotmogu && want_runners > 0 && !ctx.advice_nodes.empty())
    {
        // Kotmogu: nodes[] holds the FOUR orb spawns, but the scalar
        // enemy_flag is the ref bot's guid-hashed corner — sending both
        // runners there means one grabs and one stares at an empty spawn.
        // Assign each runner a DISTINCT orb: greedy cheapest
        // (member, unclaimed-orb) pair per round.
        std::vector<bool> orb_claimed(ctx.advice_nodes.size(), false);
        for (int r = 0; r < want_runners; ++r)
        {
            int   best_m = -1, best_o = -1;
            float best_d = 1e30f;
            for (int i = 0; i < int(ctx.members.size()); ++i)
            {
                Member const& m = ctx.members[i];
                if (m.assigned || !m.alive || m.healer || m.is_carrier)
                    continue;
                for (int o = 0; o < int(ctx.advice_nodes.size()); ++o)
                {
                    if (orb_claimed[o]) continue;
                    const float d = Dist2(m.x, m.y, ctx.advice_nodes[o].x,
                                          ctx.advice_nodes[o].y);
                    if (d < best_d) { best_d = d; best_m = i; best_o = o; }
                }
            }
            if (best_m < 0) break;
            AssignOrder(ctx.members[best_m].guid_low, BgOrder::PickupFlag,
                        ctx.advice_nodes[best_o].x, ctx.advice_nodes[best_o].y,
                        ctx.advice_nodes[best_o].z);
            ctx.members[best_m].assigned = true;
            orb_claimed[best_o] = true;
            runner_idx.push_back(best_m);
        }
    }
    else if (want_runners > 0 && PosSet(ctx.enemy_flag_x, ctx.enemy_flag_y))
    {
        for (int r = 0; r < want_runners; ++r)
        {
            // Prefer the script's FC classes (stealth in WSG/TP): try a
            // preferred-class pick first, fall back to anyone.
            int pick = -1;
            if (!ctx.fc_class_preference.empty())
            {
                float best_cost = 1e30f;
                for (int i = 0; i < int(ctx.members.size()); ++i)
                {
                    Member const& m = ctx.members[i];
                    if (m.assigned || !m.alive || m.healer || m.is_carrier)
                        continue;
                    bool preferred = false;
                    for (uint8 c : ctx.fc_class_preference)
                        if (c == m.cls) { preferred = true; break; }
                    if (!preferred) continue;
                    const float cost =
                        std::sqrt(Dist2(m.x, m.y, ctx.enemy_flag_x, ctx.enemy_flag_y));
                    if (cost < best_cost) { best_cost = cost; pick = i; }
                }
            }
            if (pick < 0)
                pick = PickNearest(ctx.members, BgOrder::PickupFlag,
                                   ctx.enemy_flag_x, ctx.enemy_flag_y,
                                   /*healer_bias=*/+1, /*allow_carrier=*/false);
            if (pick < 0) break;
            AssignOrder(ctx.members[pick].guid_low, BgOrder::PickupFlag,
                        ctx.enemy_flag_x, ctx.enemy_flag_y, ctx.enemy_flag_z);
            ctx.members[pick].assigned = true;
            runner_idx.push_back(pick);
        }
    }

    // -- Escorts -------------------------------------------------------------
    // Each live carrier gets an escort detail; the pickup runner gets one
    // too on pure-CTF maps (the run back is where flags die). First escort
    // slot prefers a healer.
    const int escorts_per_carrier = node_play ? 1 : (kotmogu ? 1 : 3);
    auto escort_to = [&](float ex, float ey, float ez, ObjectGuid focus,
                         int count, uint8 squad)
    {
        for (int e = 0; e < count; ++e)
        {
            const int pick =
                PickNearest(ctx.members, BgOrder::EscortFC, ex, ey,
                            /*healer_bias=*/e == 0 ? -1 : +1,
                            /*allow_carrier=*/false, focus);
            if (pick < 0) return;
            AssignOrder(ctx.members[pick].guid_low, BgOrder::EscortFC,
                        ex, ey, ez, focus, squad);
            ctx.members[pick].assigned = true;
        }
    };
    uint8 squad_no = 1;
    for (auto const& m : ctx.members)
        if (m.is_carrier && m.alive)
            escort_to(m.x, m.y, m.z,
                      ObjectGuid::Create<HighGuid::Player>(m.guid_low),
                      escorts_per_carrier, squad_no++);
    // A human teammate carrying the flag gets the same escort detail —
    // the executor follows the live carrier guid, so the plan-time
    // position only seeds the distance ranking.
    if (ctx.scalar_carrier_is_external && !kotmogu &&
        PosSet(ctx.friendly_carrier_x, ctx.friendly_carrier_y))
        escort_to(ctx.friendly_carrier_x, ctx.friendly_carrier_y,
                  ctx.friendly_carrier_z, ctx.friendly_carrier,
                  escorts_per_carrier, squad_no++);
    if (!node_play && !kotmogu)
        for (int ri : runner_idx)
            escort_to(ctx.members[ri].x, ctx.members[ri].y, ctx.members[ri].z,
                      ObjectGuid::Create<HighGuid::Player>(ctx.members[ri].guid_low),
                      BgTactics::RunnerEscortCount(int(ctx.members.size())), squad_no++);

    // -- EFC hunt squad (pure CTF only) --------------------------------------
    // Concentrated 3-bot intercept on the enemy carrier. On hybrid maps the
    // node planner owns the remainder; on Kotmogu killing carriers is the
    // whole midfield game and the legacy in-combat target switch (EFC-first)
    // already handles it once bodies are in the center.
    if (!node_play && !kotmogu && !ctx.enemy_carrier.IsEmpty() &&
        PosSet(ctx.enemy_carrier_x, ctx.enemy_carrier_y))
    {
        int hunters = int(ctx.members.size()) >= 8 ? 3 : 2;
        // Standoff: both flags are out. Our carrier cannot score until our
        // own flag is home, so everybody not escorting the carrier (healers
        // stay with it) goes after the enemy carrier.
        if (have_carriers + ctx.external_carriers > 0)
        {
            int free_fighters = 0;
            for (auto const& m : ctx.members)
                if (!m.assigned && m.alive && !m.healer && !m.is_carrier)
                    ++free_fighters;
            hunters = std::max(hunters, free_fighters);
        }
        for (int h = 0; h < hunters; ++h)
        {
            const int pick =
                PickNearest(ctx.members, BgOrder::HuntEFC,
                            ctx.enemy_carrier_x, ctx.enemy_carrier_y,
                            /*healer_bias=*/+1, /*allow_carrier=*/false,
                            ctx.enemy_carrier);
            if (pick < 0) break;
            // The first hunter chases the carrier directly; the others (squad 9) cut it off: the executor walks them to the point
            // ahead of the carrier on the recorded route to the enemy flag room (this order's x/y/z = where the carrier is headed).
            if (h == 0)
                AssignOrder(ctx.members[pick].guid_low, BgOrder::HuntEFC,
                            ctx.enemy_carrier_x, ctx.enemy_carrier_y,
                            ctx.enemy_carrier_z, ctx.enemy_carrier);
            else
                AssignOrder(ctx.members[pick].guid_low, BgOrder::HuntEFC,
                            PosSet(ctx.enemy_flag_x, ctx.enemy_flag_y) ? ctx.enemy_flag_x : ctx.enemy_carrier_x,
                            PosSet(ctx.enemy_flag_x, ctx.enemy_flag_y) ? ctx.enemy_flag_y : ctx.enemy_carrier_y,
                            PosSet(ctx.enemy_flag_x, ctx.enemy_flag_y) ? ctx.enemy_flag_z : ctx.enemy_carrier_z,
                            ctx.enemy_carrier, /*squad=*/9);
            ctx.members[pick].assigned = true;
        }
    }
    // -- WSG zone roles (playtest 2026-10-08: not just a straight lane between the bases) ---------------------------------------------
    // Free fighters (after carrier, runner, escorts, hunters) are split over the map: mid skirmishers at the tunnel junction, a
    // camp at the ENEMY graveyard (they take the fight to the respawn point and make the return trip hard), and a guard at our OWN
    // graveyard. The slot a bot gets rotates every few minutes of match time, so the same bot does not hold one spot all game and no
    // two matches look alike. Everyone else stays free (legacy roaming / mid pressure). Coordinates are the ike3-recorded WSG paths'
    // graveyard and tunnel points (BgWaypointData.inc).
    if (!node_play && !kotmogu && ctx.type_id == 2)
    {
        struct Zone { float x, y, z; };
        constexpr Zone kMid{1233.88f, 1476.29f, 308.015f};
        constexpr Zone kHordeGy{1029.14f, 1387.49f, 340.836f};
        constexpr Zone kAllyGy{1415.33f, 1554.79f, 343.156f};
        Zone const enemy_gy = ctx.team == 1 ? kHordeGy : kAllyGy;
        Zone const own_gy   = ctx.team == 1 ? kAllyGy : kHordeGy;
        std::vector<int> free_idx;
        for (int i = 0; i < int(ctx.members.size()); ++i)
        {
            Member const& m = ctx.members[i];
            if (!m.assigned && m.alive && !m.healer && !m.is_carrier) free_idx.push_back(i);
        }
        uint32 const epoch = ctx.in_progress_ms / (3u * 60u * 1000u);
        int const n = int(free_idx.size());
        int const n_mid = n >= 3 ? std::max(1, n * 3 / 10) : 0;
        // Graveyard camping comes in waves, not all game: every third epoch (offset per match) the camp doubles, so players coming
        // back from the graveyard sometimes meet a wall and sometimes get out.
        bool const camp_wave = ((epoch + ctx.instance_id) % 3u) == 1u;
        int const n_camp = n >= 4 ? std::max(1, n * (camp_wave ? 4 : 2) / 10) : 0;
        int const n_own = n >= 5 ? 1 : 0;
        // Rotation: order the free bots by a hash that changes with the epoch, take the first slots for each zone.
        std::sort(free_idx.begin(), free_idx.end(), [&](int a, int b)
        {
            auto key = [&](int i)
            {
                uint64 h = (ctx.members[i].guid_low + 1u) * 0x9E3779B97F4A7C15ull ^ (uint64(epoch) + 1u) * 0xC2B2AE3D27D4EB4Full;
                h ^= h >> 29; h *= 0xBF58476D1CE4E5B9ull; h ^= h >> 32;
                return h;
            };
            return key(a) < key(b);
        });
        int k = 0;
        auto give = [&](Zone const& z, int count, uint8 squad)
        {
            for (int q = 0; q < count && k < n; ++q, ++k)
            {
                Member& m = ctx.members[free_idx[k]];
                float const ox = float(int((m.guid_low * 7u) % 15u) - 7), oy = float(int((m.guid_low * 11u) % 15u) - 7);
                AssignOrder(m.guid_low, BgOrder::AttackNode, z.x + ox, z.y + oy, z.z, ObjectGuid::Empty, squad, /*target_entry=*/0);
                m.assigned = true;
            }
        };
        give(kMid, n_mid, 3);
        give(enemy_gy, n_camp, 4);
        give(own_gy, n_own, 5);
    }
    // Remainder stays unordered on pure CTF: the legacy mid-pressure /
    // score-bias roles are already good there, and leaving them free keeps
    // graceful degradation honest.
}

// ---------------------------------------------------------------------------
// Node-race family (AB, BfG, AV, IoC, Deephaul, EotS towers, DG).
// Quota-based defense scaled by live enemy pressure, then ONE concentrated
// attack squad on the weakest takeable node — the single biggest win over
// per-bot greed, which smears attackers across every enemy node at once.
// ---------------------------------------------------------------------------

void BgTeamCoordinator::PlanNodeRace(TeamPlanContext& ctx)
{
    if (ctx.nodes.empty()) return;
    const uint8 enemy_team = ctx.team == 1 ? 2 : 1;

    const bool turtle = ctx.score_delta >= ctx.score_bias_threshold ||
                        (ctx.in_progress_ms > 18u * 60u * 1000u && ctx.score_delta > 0);
    const bool all_in = ctx.score_delta <= -ctx.score_bias_threshold ||
                        (ctx.in_progress_ms > 18u * 60u * 1000u && ctx.score_delta < 0);

    auto enemy_near = [&](BgNodeState const& n) -> int
    { return ctx.team == 1 ? n.horde_players_near : n.alliance_players_near; };

    // -- Defense demands ------------------------------------------------------
    // CONTESTED SEMANTICS (builder contract, BotSnapshotBuilder ~6206):
    // a contested node's owner_team names the team DOING THE FLIP, not the
    // team that held it. So:
    //   owner==enemy && contested  -> the enemy is capping something
    //                                 (our node or a neutral): STOP THE CAP.
    //   owner==us    && contested  -> WE are mid-flip: that's an attack-
    //                                 reinforce candidate, NOT a defense.
    //   owner==us    && !contested -> held node: standing garrison.
    // Emergencies first, then garrisons sized to live enemy pressure.
    // All-in strips garrisons to a single sentry; turtle adds one.
    struct Demand { float x, y, z; int quota; bool emergency; uint32 contested_ms; };
    std::vector<Demand> defense;
    for (auto const& n : ctx.nodes)
    {
        // How long the enemy has been capping this node (first plan that saw it contested); cleared when it stops being contested.
        uint64 const cs_key = (uint64(ctx.instance_id) << 40) ^ (uint64(ctx.team) << 36) ^
                              (uint64(uint32(int32(n.x * 4.f))) * 73856093ull) ^ (uint64(uint32(int32(n.y * 4.f))) * 19349663ull);
        uint32 contested_ms = 0;
        if (n.owner_team == enemy_team && n.is_contested)
        {
            auto it = contest_since_.emplace(cs_key, ctx.in_progress_ms).first;
            contested_ms = ctx.in_progress_ms >= it->second ? ctx.in_progress_ms - it->second : 0u;
        }
        else
            contest_since_.erase(cs_key);
        if (n.is_destroyed) continue;   // razed AV tower — nothing to hold
        if (n.owner_team == enemy_team && n.is_contested)
            defense.push_back({n.x, n.y, n.z,
                               std::max(2, std::min(enemy_near(n) + 1, 4)),
                               true, contested_ms});
        else if (n.owner_team == ctx.team && !n.is_contested)
        {
            int quota = all_in ? 1 : std::clamp(enemy_near(n), 1, 3);
            if (turtle) ++quota;
            defense.push_back({n.x, n.y, n.z, quota, false, 0u});
        }
        else if (n.owner_team == ctx.team && n.is_contested)
        {
            // OUR assault in progress (AB 60 s, AV 5 min): one enemy click
            // undoes it, so a guard holds the banner while the attack squad
            // moves on to the next node instead of standing on this one for
            // the whole timer.
            defense.push_back({n.x, n.y, n.z,
                               std::clamp(enemy_near(n) + 1, 1, 3), false, 0u});
        }
    }
    std::stable_sort(defense.begin(), defense.end(),
                     [](Demand const& a, Demand const& b)
                     { return a.emergency > b.emergency; });

    // AV open-field brawl share (playtest 2026-10-08), decided here so it can shrink the garrison budget and
    // used further down to send the bots (see "AV open-field brawl"). Old Classic AV: a big part of each side
    // farmed kills in the valley while the rest took towers, so games lasted. The share is a random 25-55%
    // per match and team (hash of instance + team: stable for the whole match, no RNG state) and follows the
    // humans: one fighting within kMidHumanRadius of the zone raises it (+30 points, cap 85%), one pushing
    // the enemy side of the valley drops it to 10% so the rest push with them. A mode change must hold for
    // kMidModeHoldMs. 0 outside AV, during the endgame push (decided below) and after kMidMaxMs of match time.
    float mid_share = 0.f;
    int   alive_total = 0;
    for (auto const& m : ctx.members)
        if (m.alive && !m.is_carrier) ++alive_total;
    uint64 const mid_key = (uint64(ctx.instance_id) << 2) | uint64(ctx.team);
    // playtest 2026-10-11: the valley brawl is OFF. Bots piled up idle in AV, and he asked to drop the fight in the middle in case it was the
    // cause; AV bots now play the way ike3 / mod-playerbots do (towers, graveyards, captain, general, mines). Flip to true to bring it back.
    constexpr bool kAvValleyBrawl = false;
    if (kAvValleyBrawl && ctx.type_id == 1 && PosSet(ctx.fight_x, ctx.fight_y) && !all_in && !ctx.endgame_unconditional &&
        ctx.in_progress_ms < kMidMaxMs)
    {
        MidState& ms = mid_state_[mid_key];
        if (ms.wanted_since_ms == 0)
        {
            uint64 h = (uint64(ctx.instance_id) * 0x9E3779B97F4A7C15ull) ^ (uint64(ctx.team) * 0xC2B2AE3D27D4EB4Full);
            h ^= h >> 29; h *= 0xBF58476D1CE4E5B9ull; h ^= h >> 32;
            ms.base = 0.25f + 0.30f * float(h % 1000u) / 999.f;
            ms.wanted_since_ms = 1;
        }
        uint8 wanted = 0;
        if (!ctx.humans.empty())
        {
            // Direction of the enemy base from the zone (the script's endgame target).
            float ux = ctx.endgame_x - ctx.fight_x, uy = ctx.endgame_y - ctx.fight_y;
            float const ul = std::sqrt(ux * ux + uy * uy);
            if (ul > 1.f) { ux /= ul; uy /= ul; }
            int fighting = 0, pushing = 0;
            for (auto const& hp : ctx.humans)
            {
                float const d = std::sqrt(Dist2(hp.first, hp.second, ctx.fight_x, ctx.fight_y));
                float const along = (hp.first - ctx.fight_x) * ux + (hp.second - ctx.fight_y) * uy;
                if (d <= kMidHumanRadius) ++fighting;
                else if (along > kMidHumanRadius) ++pushing;
            }
            if (fighting > pushing) wanted = 1;
            else if (pushing > fighting) wanted = 2;
        }
        if (wanted == ms.mode)
            ms.wanted = wanted;
        else if (wanted != ms.wanted)
        {
            ms.wanted = wanted;
            ms.wanted_since_ms = std::max<uint32>(ctx.in_progress_ms, 2u);
        }
        else if (ctx.in_progress_ms >= ms.wanted_since_ms + kMidModeHoldMs)
            ms.mode = wanted;
        mid_share = ms.base;
        if (ms.mode == 1) mid_share = std::min(0.85f, ms.base + 0.30f);
        else if (ms.mode == 2) mid_share = 0.10f;
    }
    else if (ctx.type_id == 1)
        mid_state_.erase(mid_key);

    // Defense BUDGET: demands must never consume the whole roster — on
    // epic maps (AV: 15 nodes, ~7 held per side at start) unbudgeted
    // garrisons would drain all 40 bots and the attack section would
    // never run, turning the team permanently passive. Garrisons may
    // take ~60% of the live roster; emergencies may borrow up to ~75%;
    // the rest is the guaranteed attacker core.
    int alive_free = 0;
    for (auto const& m : ctx.members)
        if (m.alive && !m.assigned && !m.is_carrier) ++alive_free;
    // The brawlers come out of the garrisons first: a bot fighting in the valley is not standing on a tower.
    const int garrison_budget  = std::max(1, int(float(alive_free) * 0.6f * (1.0f - 0.6f * mid_share)));
    const int emergency_budget = std::max(1, (alive_free * 3) / 4);
    int spent = 0;
    // AV defender flip (mod-playerbots BGT 1919-1931): a bot already deep in the enemy half does not garrison; it keeps free for the attack.
    // Chance 20 + 15 per own node razed, rolled per bot per minute (stable inside the minute). Emergencies still take whoever is nearest.
    std::vector<int> flipped;
    if (ctx.type_id == 1)
    {
        int own_razed = 0;
        for (auto const& n : ctx.nodes)
            if (n.is_destroyed && n.owner_team == ctx.team) ++own_razed;
        for (int i = 0; i < int(ctx.members.size()); ++i)
        {
            Member const& m = ctx.members[i];
            if (m.assigned || !m.alive || m.is_carrier || !BgAvPlan::DefenderPastLine(ctx.team, m.x)) continue;
            uint32 const h = uint32((m.guid_low * 2654435761ull) ^ (uint64(ctx.in_progress_ms / 60000u) * 40503ull)) >> 8;
            if (int(h % 100u) < BgAvPlan::DefenderFlipChance(own_razed)) flipped.push_back(i);
        }
    }
    // Hide the flipped bots from the garrison picks; emergencies see them (and a pick stays assigned when the mask lifts).
    std::vector<int> masked;
    auto mask_flipped = [&](bool on)
    {
        if (on) { masked.clear(); for (int i : flipped) if (!ctx.members[i].assigned) { ctx.members[i].assigned = true; masked.push_back(i); } }
        else    { for (int i : masked) ctx.members[i].assigned = false; masked.clear(); }
    };
    for (auto const& d : defense)
    {
        mask_flipped(!d.emergency);
        const int budget = d.emergency ? emergency_budget : garrison_budget;
        for (int q = 0; q < d.quota && spent < budget; ++q)
        {
            // Emergencies take healers if that's what's left; garrisons don't.
            const int pick = PickNearest(ctx.members, BgOrder::DefendNode,
                                         d.x, d.y,
                                         /*healer_bias=*/d.emergency ? 0 : +1,
                                         /*allow_carrier=*/false);
            if (pick < 0) { mask_flipped(false); return; }   // out of bodies entirely
            // Stop-the-cap only when the nearest bot can still get there before the node flips; the next plan sends the attack squad
            // to retake it (mod-playerbots/ike3 send whoever rolled the node, however far).
            if (d.emergency &&
                !BgTactics::CanArriveBeforeFlip(std::sqrt(Dist2(ctx.members[pick].x, ctx.members[pick].y, d.x, d.y)),
                                                BgTactics::CaptureWindowMs(ctx.type_id), d.contested_ms))
                break;
            AssignOrder(ctx.members[pick].guid_low, BgOrder::DefendNode,
                        d.x, d.y, d.z);
            ctx.members[pick].assigned = true;
            ++spent;
        }
    }

    mask_flipped(false);

    // -- AB: all five nodes ours and nobody capping: camp the enemy graveyard --------------------------------------------------
    // (mod-playerbots BattleGroundTactics: a team that owns everything camps the enemy graveyard.) Standing on banners that nobody
    // is attacking wins nothing more; the camp keeps their respawns from walking back out and holds the node count. A few bots, not
    // everyone: the garrisons above stay. Positions: the enemy side's spawn point from ArathiBasinScript (with z).
    if (ctx.type_id == 3)
    {
        int owned = 0, contested = 0, total = 0;
        for (auto const& n : ctx.nodes)
        {
            if (n.is_destroyed) continue;
            ++total;
            if (n.owner_team == ctx.team) ++owned;
            if (n.is_contested) ++contested;
        }
        if (BgTactics::CampEnemyGraveyard(owned, total, contested))
        {
            // Alliance (team 1) camps the Horde side and the other way round.
            float const gx = ctx.team == 1 ? 686.57f : 1285.96f;
            float const gy = ctx.team == 1 ? 683.04f : 1281.62f;
            float const gz = ctx.team == 1 ? -12.59f : -15.67f;
            int free_fighters = 0;
            for (auto const& m : ctx.members)
                if (!m.assigned && m.alive && !m.healer && !m.is_carrier) ++free_fighters;
            int const camp = free_fighters >= 5 ? std::max(2, free_fighters * 4 / 10) : 0;
            for (int q = 0; q < camp; ++q)
            {
                int const pick = PickNearest(ctx.members, BgOrder::AttackNode, gx, gy, /*healer_bias=*/+1, /*allow_carrier=*/false);
                if (pick < 0) break;
                uint64 const g = ctx.members[pick].guid_low;
                float const ox = float(int((g * 7u) % 15u) - 7), oy = float(int((g * 11u) % 15u) - 7);
                AssignOrder(g, BgOrder::AttackNode, gx + ox, gy + oy, gz, ObjectGuid::Empty, /*squad=*/4, /*target_entry=*/0);
                ctx.members[pick].assigned = true;
            }
        }
    }

    // -- Attack: one concentrated squad --------------------------------------
    // Target = cheapest takeable node: neutral beats enemy-held, light
    // defense beats heavy, script priority (AV towers) breaks ties, distance
    // from the unassigned force breaks the rest. Endgame coords override the
    // target when the all-in switch is thrown and the script has one.
    float cx = 0.f, cy = 0.f;
    int   free_count = 0;
    for (auto const& m : ctx.members)
        if (!m.assigned && m.alive) { cx += m.x; cy += m.y; ++free_count; }
    if (free_count == 0) return;
    cx /= float(free_count); cy /= float(free_count);

    float tx = 0.f, ty = 0.f, tz = 0.f;
    uint8  order_kind   = BgOrder::AttackNode;
    uint32 push_entry   = 0;
    // Push the enemy boss when LOSING (all_in) OR when the script flags the
    // push unconditional (AV: enemy reinforcements are low — the boss kill
    // ends the match regardless of node count, so push even while winning).
    // Stage the enemy CAPTAIN (priority-3 node) before the general: the
    // general's room is only deliberately cleared once the captain is dead.
    // Also commit a clearly-LEADING team to the captain->general push once the
    // early game is past: killing the enemy general is an instant win, so the
    // winning team should drive for it rather than wait for the slow AV
    // reinforcement race to drain below endgame_unconditional (which stalls in
    // bot-only matches around a 1-tower / +75 lead). The losing team keeps
    // contesting nodes to defend.
    const bool lead_push = ctx.endgame_creature_entry != 0 &&
                           ctx.score_delta >= 50 &&
                           ctx.in_progress_ms > 4u * 60u * 1000u;
    // STICKY endgame commit. lead_push flips off the instant the reinforcement
    // race nudges the lead below +50, which in bot-only AV happens constantly
    // around the +-75 one-tower stall — so the captain push kept starting and
    // stopping and never sustained long enough to burn the captain down. Once a
    // team commits (lead_push fires while an endgame creature exists), latch the
    // commitment and keep pushing until the endgame creature is dead (the AV
    // script stops advertising it -> endgame_creature_entry==0) or the team is
    // genuinely crushed (score_delta < -150, i.e. about to lose — fall back to
    // defending). This is also how humans break the stall: the leading team
    // drives the captain->general kill instead of trading nodes forever.
    const uint64 commit_key = (uint64(ctx.instance_id) << 2) | uint64(ctx.team);
    if (lead_push)
        endgame_commit_[commit_key] = ctx.in_progress_ms;
    const bool committed = endgame_commit_.find(commit_key) != endgame_commit_.end();
    if (ctx.endgame_creature_entry == 0 || ctx.score_delta < -150)
        endgame_commit_.erase(commit_key);  // endgame done / team collapsing
    if (!ctx.captain_alive)
        { captain_go_.erase(commit_key); captain_wait_.erase(commit_key); }      // captain dead: the next assault starts from the staging spot again
    const bool sticky_push = committed && ctx.endgame_creature_entry != 0 &&
                             ctx.score_delta >= -150;
    const bool push_boss = all_in || ctx.endgame_unconditional || lead_push ||
                           sticky_push;

    // Enemy towers / bunkers still standing (AV): the general keeps their buffs, so the general push waits until they are down
    // (ike3's rule). A tower is a priority-2 script node held by the enemy that is not razed.
    int enemy_towers_up = 0;
    if (ctx.endgame_needs_towers)
        for (auto const& n : ctx.nodes)
            if (!n.is_destroyed && n.owner_team == enemy_team && NodePriorityFor(ctx, n) == 2)
                ++enemy_towers_up;
    // General rule: ike3 (all towers down) or mod-playerbots (two razed and the last graveyard held or 20 friends near the boss): BgAvPlan::GeneralPushOk.
    int enemy_towers_razed = 0;
    bool own_final_gy = false;
    {
        auto const fg = BgAvPlan::FinalGraveyard(ctx.team);
        for (auto const& n : ctx.nodes)
        {
            if (n.is_destroyed && n.owner_team == enemy_team && NodePriorityFor(ctx, n) == 2) ++enemy_towers_razed;
            if (!n.is_destroyed && n.owner_team == ctx.team && !n.is_contested && Dist2(n.x, n.y, fg.x, fg.y) < 30.f * 30.f) own_final_gy = true;
        }
    }
    int friends_near_boss = 0;
    if (PosSet(ctx.endgame_x, ctx.endgame_y))
        for (auto const& m : ctx.members)
            if (m.alive && Dist2(m.x, m.y, ctx.endgame_x, ctx.endgame_y) < 200.f * 200.f) ++friends_near_boss;
    bool const general_ok = BgAvPlan::GeneralPushOk(enemy_towers_up, enemy_towers_razed, own_final_gy, friends_near_boss);

    // -- AV open-field brawl (playtest 2026-10-08) -------------------------------
    // Send the brawlers (share decided above) to the fight zone as ordinary AttackNode orders. The brawl ends once
    // the push for the enemy captain/general is on (a lead, a deficit, low enemy reinforcements), so matches end.
    // The brawl also goes on while the general push is held back by standing enemy towers and no captain is left to go for.
    bool const push_held = ctx.endgame_needs_towers && !general_ok && !(ctx.captain_alive && PosSet(ctx.captain_x, ctx.captain_y));
    if (mid_share > 0.f && (!push_boss || push_held))
    {
        int free_n = 0;
        for (auto const& m : ctx.members)
            if (!m.assigned && m.alive && !m.is_carrier) ++free_n;
        // The squad keeps at least 30% of the free bots so towers still fall.
        int const to_mid = std::min(int(mid_share * float(alive_total) + 0.5f), (free_n * 7) / 10);
        for (int q = 0; q < to_mid; ++q)
        {
            int const pick = PickNearest(ctx.members, BgOrder::AttackNode, ctx.fight_x, ctx.fight_y, /*healer_bias=*/0,
                                         /*allow_carrier=*/false);
            if (pick < 0) break;
            // Small per-bot offset (inside the 8 y arrival radius) so the brawlers do not stack on one point.
            uint64 const g = ctx.members[pick].guid_low;
            float const ox = float(int((g * 7u) % 13u) - 6), oy = float(int((g * 11u) % 13u) - 6);
            AssignOrder(g, BgOrder::AttackNode, ctx.fight_x + ox, ctx.fight_y + oy, ctx.fight_z,
                        ObjectGuid::Empty, /*squad=*/2, /*target_entry=*/0);
            ctx.members[pick].assigned = true;
        }
    }

    // AV Snowfall Graveyard detail (ike3 "supporters", mod-playerbots Snowfall rule): while the neutral graveyard in the middle of the valley is
    // not ours, a share of the free bots takes it. Not during the general push.
    if (ctx.type_id == 1 && (!push_boss || push_held))
        for (auto const& n : ctx.nodes)
        {
            float const sdx = n.x - BgAvPlan::kSnowfallX, sdy = n.y - BgAvPlan::kSnowfallY;
            if (sdx * sdx + sdy * sdy > 30.0f * 30.0f) continue;
            if (n.is_destroyed || n.owner_team == ctx.team || (n.owner_team == enemy_team && n.is_contested)) break;
            int free_n = 0;
            for (auto const& m : ctx.members)
                if (!m.assigned && m.alive && !m.is_carrier) ++free_n;
            int const k = BgAvPlan::SnowfallDetail(free_n, ctx.in_progress_ms);
            for (int q = 0; q < k; ++q)
            {
                int const pick = PickNearest(ctx.members, BgOrder::AttackNode, n.x, n.y, /*healer_bias=*/+1, /*allow_carrier=*/false);
                if (pick < 0) break;
                AssignOrder(ctx.members[pick].guid_low, BgOrder::AttackNode, n.x, n.y, n.z, ObjectGuid::Empty, /*squad=*/2, /*target_entry=*/0);
                ctx.members[pick].assigned = true;
            }
            break;
        }

    // AV mine bosses (ike3 mine-boss objective): a few bots kill the boss of every mine the team does not own. Not during the push.
    if (!push_boss || push_held)
        for (auto const& kt : ctx.kill_targets)
            for (int q = 0; q < 3; ++q)
            {
                int const pick = PickNearest(ctx.members, BgOrder::PushEndgame, kt.x, kt.y, /*healer_bias=*/+1, /*allow_carrier=*/false);
                if (pick < 0) break;
                AssignOrder(ctx.members[pick].guid_low, BgOrder::PushEndgame, kt.x, kt.y, kt.z, ObjectGuid::Empty, /*squad=*/4, kt.entry);
                ctx.members[pick].assigned = true;
            }

    // ike3 / mod-playerbots: the captain comes first. Non-supporters (ike3 60-80 %) or 90 % of everyone (mod-playerbots) walk to the enemy
    // captain while it lives, gathering at the staging spot first; the towers follow once it is down. So the captain squad starts without
    // the lead / deficit trigger the general push needs.
    bool const captain_first = ctx.type_id == 1 && ctx.captain_alive && PosSet(ctx.captain_x, ctx.captain_y);
    if ((push_boss || captain_first) && ctx.captain_alive && PosSet(ctx.captain_x, ctx.captain_y))
    {
        // Captain: the squad gathers at the staging spot and goes in together once five of the team stand there (ike3: engage once 5
        // attackers are at the waiting spot). The decision latches until the captain is dead.
        bool go = true;
        if (PosSet(ctx.captain_stage_x, ctx.captain_stage_y))
        {
            if (captain_go_.find(commit_key) == captain_go_.end())
            {
                int at_stage = 0;
                for (auto const& m : ctx.members)
                    if (m.alive && Dist2(m.x, m.y, ctx.captain_stage_x, ctx.captain_stage_y) < 60.f * 60.f) ++at_stage;
                uint32 const waited_since = captain_wait_.emplace(commit_key, ctx.in_progress_ms).first->second;
                uint32 const waited = ctx.in_progress_ms >= waited_since ? ctx.in_progress_ms - waited_since : 0u;
                if (BgAvPlan::CaptainGather(at_stage, false, waited)) captain_go_[commit_key] = ctx.in_progress_ms;
            }
            go = captain_go_.find(commit_key) != captain_go_.end();
        }
        if (go)
        {
            tx = ctx.captain_x; ty = ctx.captain_y; tz = ctx.captain_z;
            order_kind = BgOrder::PushEndgame;
            push_entry = ctx.captain_creature_entry;
        }
        else
        {
            tx = ctx.captain_stage_x; ty = ctx.captain_stage_y; tz = ctx.captain_stage_z;
            order_kind = BgOrder::AttackNode;
        }
    }
    else if (push_boss && general_ok && PosSet(ctx.endgame_x, ctx.endgame_y))
    {
        tx = ctx.endgame_x; ty = ctx.endgame_y; tz = ctx.endgame_z;
        order_kind = BgOrder::PushEndgame;
        push_entry = ctx.endgame_creature_entry;
    }
    else
    {
        float best_cost = 1e30f;
        struct SplitCand { float cost, x, y, z; int weight; };
        std::vector<SplitCand> split_cands;
        for (auto const& n : ctx.nodes)
        {
            // Skip razed structures, held garrison nodes and enemy-flip
            // emergencies (the defense demands above own those).
            // Candidates: neutral, enemy-held uncontested, and OUR
            // flip-in-progress (owner==us && contested — the cheapest
            // finish of all, reinforce it).
            if (n.is_destroyed) continue;
            if (n.owner_team == ctx.team) continue;   // held, or our assault (guarded above)
            if (n.owner_team == enemy_team && n.is_contested) continue;
            const bool neutral = n.owner_team == 0;
            const bool enemy   = n.owner_team == enemy_team;
            float cost = std::sqrt(Dist2(cx, cy, n.x, n.y)) * 0.05f;
            if (enemy)   cost += 8.f;
            if (neutral) cost += 2.f;
            cost += float(enemy_near(n)) * 3.f;
            // Find the matching advice-node priority (AV towers > GYs).
            cost -= float(NodePriorityFor(ctx, n)) * 2.f;
            split_cands.push_back({cost, n.x, n.y, n.z, 2 + int(NodePriorityFor(ctx, n))});
            if (cost < best_cost)
            { best_cost = cost; tx = n.x; ty = n.y; tz = n.z; }
        }
        // Node-race split (WSG-free maps with 4+ nodes, i.e. AB/Gilneas style): the old single concentrated squad sent the whole
        // team Stables -> Blacksmith across the river and nobody took the Lumber Mill, Farm or Mine (playtest, AB test 2026-10-08).
        // Spread the free attackers over up to four takeable nodes in proportion to the node weight (script priority + 2, so the
        // Blacksmith pivot gets the biggest group), keep about one in eight unordered as roamers. Held nodes keep their garrison
        // from the defense pass above; a node that is lost or attacked turns up in the next plan and is re-assigned (the sticky
        // discount in PickNearest keeps the others where they are).
        // Alterac Valley (ike3: "split the team to capture two towers at the same time"): only the towers and bunkers (weight 4) are
        // candidates and the free attackers form at most two groups on the two cheapest of them; with fewer than two the single
        // squad plan below stays. The brawl share above and the garrisons are untouched.
        bool const av = ctx.type_id == 1;
        if (av)
        {
            // ike3 / mod-playerbots: walk the enemy's node list in order (graveyards, bunkers, towers), the first two that are still worth
            // attacking, one group each (BgAvPlan::PickAttackNodes). Replaces the cost-based towers-only pick, which never took a graveyard.
            std::vector<BgAvPlan::AvNodeView> views;
            views.reserve(ctx.nodes.size());
            for (auto const& n : ctx.nodes)
                views.push_back({ n.x, n.y, n.owner_team == ctx.team, n.is_destroyed, n.owner_team == enemy_team && n.is_contested });
            split_cands.clear();
            int rank = 0;
            for (int idx : BgAvPlan::PickAttackNodes(ctx.team, views, 2))
            {
                BgNodeState const& n = ctx.nodes[idx];
                split_cands.push_back({ float(rank), n.x, n.y, n.z, rank == 0 ? 5 : 3 });
                if (rank == 0) { tx = n.x; ty = n.y; tz = n.z; best_cost = 0.f; }
                ++rank;
            }
        }
        if (split_cands.size() >= 2 && ctx.nodes.size() >= 4)
        {
            // Variety (the owner: no two matches alike): the focus rotates. Every 4 minutes of match time, per match, each candidate's
            // cost and weight are nudged by a hash of its position, so the main squad is not always on the same pivot node and a
            // second node regularly gets a real group.
            uint32 const var_epoch = ctx.in_progress_ms / (4u * 60u * 1000u) + ctx.instance_id * 7u;
            for (auto& c : split_cands)
            {
                if (av) break;   // AV keeps the list order (BgAvPlan::PickAttackNodes)
                uint32 h = uint32(int32(c.x)) * 2654435761u ^ uint32(int32(c.y)) * 40503u ^ (var_epoch + 1u) * 2246822519u;
                h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
                c.cost += float(h % 400u) / 100.0f - 2.0f;      // +-2, against the 8 / 2 / 0.05-per-yard cost terms
                c.weight += int((h >> 12) % 3u);
            }
            std::sort(split_cands.begin(), split_cands.end(),
                      [](SplitCand const& a, SplitCand const& b) { return a.cost < b.cost; });
            if (split_cands.size() > (av ? 2u : 4u)) split_cands.resize(av ? 2u : 4u);
            int free_n = 0, weight_sum = 0;
            for (auto const& m : ctx.members)
                if (!m.assigned && m.alive && !m.is_carrier) ++free_n;
            for (auto const& c : split_cands) weight_sum += c.weight;
            int const to_assign = free_n;
            for (auto const& c : split_cands)
            {
                int const quota = std::max(1, (to_assign * c.weight) / std::max(1, weight_sum));
                // Sticky pass first: bots already attacking this node keep it (up to the quota). The nearest-first greedy pick below
                // otherwise lets an earlier node steal a bot that is walking to a later one, and the squads swap places every plan.
                int kept = 0;
                for (auto& m : ctx.members)
                {
                    if (kept >= quota) break;
                    if (m.assigned || !m.alive || m.is_carrier) continue;
                    auto prev = orders_.find(m.guid_low);
                    if (prev == orders_.end() || prev->second.kind != BgOrder::AttackNode || prev->second.squad != 1) continue;
                    if (Dist2(prev->second.x, prev->second.y, c.x, c.y) >= kStickyRadius * kStickyRadius) continue;
                    AssignOrder(m.guid_low, BgOrder::AttackNode, c.x, c.y, c.z, ObjectGuid::Empty, /*squad=*/1, /*target_entry=*/0);
                    m.assigned = true;
                    ++kept;
                }
                for (int q = kept; q < quota; ++q)
                {
                    int const pick = PickNearest(ctx.members, BgOrder::AttackNode, c.x, c.y, /*healer_bias=*/+1,
                                                 /*allow_carrier=*/false);
                    if (pick < 0) return;
                    AssignOrder(ctx.members[pick].guid_low, BgOrder::AttackNode, c.x, c.y, c.z,
                                ObjectGuid::Empty, /*squad=*/1, /*target_entry=*/0);
                    ctx.members[pick].assigned = true;
                }
            }
            // Road skirmishers (the owner: fights on the roads between the nodes, not only at the banners). About a fifth of the bots still
            // free go to a point midway between two of the contested nodes, picked by a hash that rotates every 3 minutes, and fight
            // whatever they meet on the way and there. Their orders are AttackNode at the road point; the 40 y engage does the rest.
            if (split_cands.size() >= 2 && !av)   // AV: no road skirmishers (ike3 / mod-playerbots have none; playtest 2026-10-11)
            {
                int left = 0;
                for (auto const& m : ctx.members)
                    if (!m.assigned && m.alive && !m.is_carrier) ++left;
                int const n_road = left / 5;
                uint32 const road_epoch = ctx.in_progress_ms / (3u * 60u * 1000u) + ctx.instance_id * 13u;
                for (int q = 0; q < n_road; ++q)
                {
                    uint32 h = (road_epoch + 1u) * 2654435761u ^ uint32(q + 1) * 40503u ^ uint32(ctx.team) * 2246822519u;
                    h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
                    size_t const ia = h % split_cands.size();
                    size_t ib = (h >> 8) % split_cands.size();
                    if (ib == ia) ib = (ib + 1) % split_cands.size();
                    float const t = 0.35f + 0.30f * float((h >> 16) % 100u) / 99.0f;     // somewhere in the middle of the road
                    float const rx = split_cands[ia].x + (split_cands[ib].x - split_cands[ia].x) * t;
                    float const ry = split_cands[ia].y + (split_cands[ib].y - split_cands[ia].y) * t;
                    float const rz = split_cands[ia].z + (split_cands[ib].z - split_cands[ia].z) * t;
                    int const pick = PickNearest(ctx.members, BgOrder::AttackNode, rx, ry, /*healer_bias=*/+1, /*allow_carrier=*/false);
                    if (pick < 0) break;
                    AssignOrder(ctx.members[pick].guid_low, BgOrder::AttackNode, rx, ry, rz, ObjectGuid::Empty, /*squad=*/6, /*target_entry=*/0);
                    ctx.members[pick].assigned = true;
                }
            }
            // Rounding leaves a few bots without a quota: every bot gets an order (AB test: three per side stood at the start),
            // the leftovers go round-robin over the same nodes, nearest bot first.
            for (size_t k = 0;; ++k)
            {
                SplitCand const& c = split_cands[k % split_cands.size()];
                int const pick = PickNearest(ctx.members, BgOrder::AttackNode, c.x, c.y, /*healer_bias=*/+1,
                                             /*allow_carrier=*/false);
                if (pick < 0) break;
                AssignOrder(ctx.members[pick].guid_low, BgOrder::AttackNode, c.x, c.y, c.z,
                            ObjectGuid::Empty, /*squad=*/1, /*target_entry=*/0);
                ctx.members[pick].assigned = true;
            }
            return;
        }
        if (!PosSet(tx, ty))
        {
            // We own everything (or no takeable node). AV (mod-playerbots AV_BOSS_WAIT): wait next to the enemy general, so the team is
            // in place when the push opens. Elsewhere leave the remainder unordered; legacy roamers patrol well.
            if (ctx.type_id == 1)
            {
                auto const bw = BgAvPlan::BossWait(ctx.team);
                for (auto& m : ctx.members)
                {
                    if (m.assigned || !m.alive || m.is_carrier) continue;
                    float const ox = float(int((m.guid_low * 7u) % 11u) - 5), oy = float(int((m.guid_low * 11u) % 11u) - 5);
                    AssignOrder(m.guid_low, BgOrder::AttackNode, bw.x + ox, bw.y + oy, bw.z, ObjectGuid::Empty, /*squad=*/5, /*target_entry=*/0);
                    m.assigned = true;
                }
            }
            return;
        }
    }

    for (auto& m : ctx.members)
    {
        if (m.assigned || !m.alive || m.is_carrier) continue;
        AssignOrder(m.guid_low, order_kind, tx, ty, tz, ObjectGuid::Empty,
                    /*squad=*/1, /*target_entry=*/push_entry);
        m.assigned = true;
    }
}

// Match a live node against the script's static node list to read its
// priority weight (position match within 25y — banner GO vs advice coords
// are a few yards apart on most maps).
uint8 BgTeamCoordinator::NodePriorityFor(TeamPlanContext const& ctx,
                                         BgNodeState const& n)
{
    for (auto const& an : ctx.advice_nodes)
        if (Dist2(an.x, an.y, n.x, n.y) < 25.f * 25.f)
            return an.priority;
    return 0;
}

// ---------------------------------------------------------------------------
// Per-team plan entry: family selection + diagnostics.
// ---------------------------------------------------------------------------

void BgTeamCoordinator::PlanTeam(TeamPlanContext& ctx, uint32 /*now_ms*/)
{
    const bool flag_play = PosSet(ctx.enemy_flag_x, ctx.enemy_flag_y) ||
                           !ctx.enemy_carrier.IsEmpty() ||
                           std::any_of(ctx.members.begin(), ctx.members.end(),
                                       [](Member const& m) { return m.is_carrier; });
    const bool node_play = !ctx.nodes.empty();

    if (flag_play)
        PlanCtf(ctx);          // also covers the flag layer of hybrids
    if (node_play)
        PlanNodeRace(ctx);     // remaining members

    // Plan-change diagnostic: log when the team's attack focus or carrier
    // detail changed since the last plan (not every 750ms tick).
    // The signature describes WHAT the team is doing (which kinds go to which objective cells), never WHO: a bot dying or
    // respawning, the registry order, the +-6 y brawl offsets and a moving fight zone all used to flip the per-bot hash and
    // re-log "plan changed" nearly every cycle ([bgcoord] churn). Collect distinct (kind, 30 y cell) objectives plus a coarse
    // squad-size bucket per objective, sort them and hash the sorted list.
    std::vector<uint64> objectives;
    int ordered = 0;
    for (auto const& m : ctx.members)
    {
        auto it = next_orders_.find(m.guid_low);
        if (it == next_orders_.end()) continue;
        ++ordered;
        // Carrier-tracking orders (escort / hunt / carry) target a MOVING player: only the kind is stable plan content.
        const bool tracks_carrier =
            it->second.kind == BgOrder::EscortFC ||
            it->second.kind == BgOrder::HuntEFC  ||
            it->second.kind == BgOrder::CarryHome;
        uint64 v = uint64(it->second.kind) << 56;
        if (!tracks_carrier)
        {
            v |= (uint64(uint32(int32(it->second.x / 30.f))) & 0xFFFFFFull) << 28;
            v |= (uint64(uint32(int32(it->second.y / 30.f))) & 0xFFFFFFull) << 4;
        }
        objectives.push_back(v);
    }
    std::sort(objectives.begin(), objectives.end());
    uint64 sig = 1469598103934665603ull;   // FNV-1a over the sorted objectives, each with a count bucket (0, 1-3, 4-7, ...)
    auto mix = [&sig](uint64 v)
    { sig ^= v; sig *= 1099511628211ull; };
    for (size_t i = 0; i < objectives.size();)
    {
        size_t j = i;
        while (j < objectives.size() && objectives[j] == objectives[i]) ++j;
        mix(objectives[i]);
        mix(uint64((j - i + 3) / 4));
        i = j;
    }
    const uint64 key = (uint64(ctx.instance_id) << 2) | ctx.team;
    auto sig_it = plan_sig_.find(key);
    if (sig_it == plan_sig_.end() || sig_it->second != sig)
    {
        plan_sig_[key] = sig;
        TC_LOG_INFO("playerbot.v2",
            "[bgcoord] bg_type={} instance={} team={} plan changed: "
            "{} bots, {} ordered, nodes={} flag_play={} score_delta={}",
            ctx.type_id, ctx.instance_id, uint32(ctx.team),
            uint32(ctx.members.size()), ordered, uint32(ctx.nodes.size()),
            flag_play ? 1 : 0, ctx.score_delta);
    }
}

// ---------------------------------------------------------------------------
// World-tick driver.
// ---------------------------------------------------------------------------

void BgTeamCoordinator::Update(uint32 now_ms)
{
    if (now_ms - last_plan_ms_ < kPlanIntervalMs)
        return;
    last_plan_ms_ = now_ms;

    if (!sConfigMgr->GetBoolDefault("Playerbot.Bg.Coordinator.Enable", true, true))
    {
        if (!orders_.empty()) orders_.clear();
        last_dump_ = "coordinator disabled (Playerbot.Bg.Coordinator.Enable=0)";
        return;
    }

    // -- Bucket every in-BG bot by (instance, team) ---------------------------
    struct Bucket
    {
        uint32 instance_id = 0;
        uint16 type_id = 0;
        uint8  team = 0;
        Battleground* bg = nullptr;
        std::vector<std::pair<uint64, Player*>> bots;
    };
    std::unordered_map<uint64, Bucket> buckets;
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const& e)
    {
        if (!e.ai) return;
        // A bot stopped with "bgrun stop" (or not armed) never runs an order (BgDispatch returns at once), so it gets no quota slot.
        if (!e.ai->bg_active()) return;
        Player* p = ObjectAccessor::FindConnectedPlayer(
                        ObjectGuid::Create<HighGuid::Player>(id));
        if (!p) return;
        Battleground* bg = p->GetBattleground();
        if (!bg || bg->isArena()) return;
        if (bg->GetStatus() != STATUS_IN_PROGRESS) return;
        const uint8 team_u8 = p->GetEffectiveTeam() == ALLIANCE ? 1 : 2;
        const uint64 key = (uint64(bg->GetInstanceID()) << 2) | team_u8;
        Bucket& b = buckets[key];
        b.instance_id = bg->GetInstanceID();
        b.type_id     = uint16(bg->GetTypeID());
        b.team        = team_u8;
        b.bg          = bg;
        b.bots.emplace_back(uint64(id), p);
    });

    next_orders_.clear();
    std::ostringstream dump;

    for (auto& [key, b] : buckets)
    {
        // Reference snapshot: the freshest member view of map-global BG
        // state (node ownership, carriers, scores). The builder harvests
        // these every snapshot build; one member's view serves the team.
        std::shared_ptr<BotSnapshot const> ref;
        for (auto const& [glow, p] : b.bots)
        {
            auto s = Services::Snapshots().latest(glow);
            if (s && s->bg.current_type_id != 0 &&
                (!ref || s->version > ref->version))
                ref = s;
        }
        if (!ref)
            continue;   // data cold (first seconds of a match) — legacy runs
        // The snapshot's RESOLVED bg type id (the builder falls back to
        // the queue's BattlemasterList id when Battleground::GetTypeID()
        // is 0) — keep family checks consistent with the advice source.
        const uint16 resolved_type_id = ref->bg.current_type_id;
        // Advice is computed HERE, on the world thread, from the immutable
        // reference snapshot. Do NOT borrow a member BotAI's BgAdviceCache:
        // that cache is OWNED BY THE bot logic WORKER THREAD — BgDispatch
        // reassigns its heap vectors every ≤2s, so reading it here could
        // iterate freed buffers (use-after-free; adversarial review
        // 2026-06-10). Scripts are stateless and registered once at boot;
        // GetAdvice on a snapshot view is thread-safe and costs one call
        // per (instance, team) per 750ms plan — negligible next to the
        // per-bot per-tick churn the bot logic-side cache exists to avoid.
        BattlegroundAdvice const adv =
            Services::Battlegrounds().GetAdvice(BotSnapshotView(*ref));

        TeamPlanContext ctx;
        ctx.team        = b.team;
        ctx.type_id     = resolved_type_id;
        ctx.instance_id = b.instance_id;
        ctx.nodes       = ref->bg.node_states;
        ctx.in_progress_ms = ref->bg.in_progress_ms;
        ctx.score_delta = b.team == 1
            ? int32(ref->bg.score_alliance) - int32(ref->bg.score_horde)
            : int32(ref->bg.score_horde)    - int32(ref->bg.score_alliance);
        ctx.score_bias_threshold =
            adv.score_bias_threshold > 0 ? adv.score_bias_threshold : 200;
        ctx.enemy_flag_x = adv.enemy_flag_x; ctx.enemy_flag_y = adv.enemy_flag_y;
        ctx.enemy_flag_z = adv.enemy_flag_z;
        ctx.own_flag_x = adv.own_flag_x; ctx.own_flag_y = adv.own_flag_y;
        ctx.own_flag_z = adv.own_flag_z;
        ctx.home_x = adv.home_base_x; ctx.home_y = adv.home_base_y;
        ctx.home_z = adv.home_base_z;
        ctx.endgame_x = adv.endgame_target_x; ctx.endgame_y = adv.endgame_target_y;
        ctx.endgame_z = adv.endgame_target_z;
        ctx.endgame_unconditional  = adv.endgame_unconditional;
        ctx.endgame_creature_entry = adv.endgame_creature_entry;
        // Enemy captain = the ENEMY-side priority-3 node the AV script still
        // advertises (it drops each captain from the list once that captain
        // dies). team 1 = Alliance hunts Galvangar 11947 (Frostwolf, the
        // -545,-165 spawn); team 2 = Horde hunts Balinda 11949 (Stormpike,
        // the -57,-286 spawn). The script lists BOTH captains with priority
        // 3, so match by spawn position — never take "the last one" or the
        // team would push toward its own captain.
        ctx.captain_creature_entry = (b.team == 1) ? 11947u : 11949u;
        const float cap_tx = (b.team == 1) ? -545.2f : -57.8f;
        const float cap_ty = (b.team == 1) ? -165.4f : -286.6f;
        for (auto const& an : adv.nodes)
            if (an.priority == 3 &&
                Dist2(an.x, an.y, cap_tx, cap_ty) < 25.f * 25.f)
            {
                ctx.captain_x = an.x; ctx.captain_y = an.y; ctx.captain_z = an.z;
                ctx.captain_alive = true;
            }
        ctx.captain_stage_x = adv.captain_stage_x; ctx.captain_stage_y = adv.captain_stage_y; ctx.captain_stage_z = adv.captain_stage_z;
        ctx.endgame_needs_towers = adv.endgame_needs_towers;
        for (auto const& kt : adv.kill_targets)
            ctx.kill_targets.push_back({kt.x, kt.y, kt.z, kt.entry});
        ctx.fight_x = adv.fight_zone_x; ctx.fight_y = adv.fight_zone_y; ctx.fight_z = adv.fight_zone_z;
        if (resolved_type_id == 1 && PosSet(ctx.fight_x, ctx.fight_y))
        {
            // AV: the open-field brawl does not sit on Snowfall all match. Every 5 minutes of match time the brawl moves to the next
            // valley hot spot (both teams read the same epoch + instance, so the armies still meet there): Snowfall GY (the script's
            // own zone), Stonehearth GY on the Alliance side of the valley, Iceblood GY on the Horde side. Coordinates are the AV
            // script's banner spawns.
            struct Spot { float x, y, z; };
            Spot const kSpots[] = { { ctx.fight_x, ctx.fight_y, ctx.fight_z }, { 73.7f, -426.0f, 61.0f }, { -612.5f, -397.0f, 61.0f } };
            uint32 const spot = (ref->bg.in_progress_ms / (5u * 60u * 1000u) + b.instance_id) % 3u;
            if (spot != 0u)
            { ctx.fight_x = kSpots[spot].x; ctx.fight_y = kSpots[spot].y; ctx.fight_z = kSpots[spot].z; }
        }
        if (PosSet(ctx.fight_x, ctx.fight_y) && b.bg)
        {
            // Living human teammates (not bucketed bots): the brawl share follows where they fight.
            Team const my_team = b.team == 1 ? ALLIANCE : HORDE;
            for (auto const& [pguid, bp] : b.bg->GetPlayers())
            {
                if (bp.Team != my_team || bp.OfflineRemoveTime) continue;
                bool is_bot = false;
                for (auto const& bot_entry : b.bots)
                    if (bot_entry.first == pguid.GetCounter()) { is_bot = true; break; }
                if (is_bot) continue;
                Player const* hp = ObjectAccessor::FindPlayer(pguid);
                if (hp && hp->IsAlive() && hp->GetMapId() == 30)
                    ctx.humans.emplace_back(hp->GetPositionX(), hp->GetPositionY());
            }
        }
        ctx.fc_class_preference = adv.fc_class_preference;
        for (auto const& an : adv.nodes)
            ctx.advice_nodes.push_back({an.x, an.y, an.z, an.priority});
        ctx.friendly_carrier   = ref->bg.friendly_flag_carrier;
        ctx.enemy_carrier      = ref->bg.enemy_flag_carrier;
        ctx.enemy_carrier_x    = ref->bg.enemy_carrier_x;
        ctx.enemy_carrier_y    = ref->bg.enemy_carrier_y;
        ctx.enemy_carrier_z    = ref->bg.enemy_carrier_z;

        ctx.members.reserve(b.bots.size());
        for (auto const& [glow, p] : b.bots)
        {
            Member m;
            m.guid_low = glow;
            m.x = p->GetPositionX(); m.y = p->GetPositionY();
            m.z = p->GetPositionZ();
            m.cls   = uint8(p->GetClass());
            m.alive = p->IsAlive();
            uint16 spec = 0;
            if (auto s = Services::Snapshots().latest(glow))
                spec = uint16(s->identity.spec);
            m.healer = IsHealerSpec(m.cls, spec);
            m.tank   = IsTankSpec(m.cls, spec);
            ObjectGuid const pg = p->GetGUID();
            m.is_carrier = pg == ref->bg.friendly_flag_carrier;
            if (!m.is_carrier)
                for (ObjectGuid const& cg : ref->bg.all_friendly_carriers)
                    if (cg == pg) { m.is_carrier = true; break; }
            ctx.members.push_back(m);
        }
        // Deterministic order: assignment must not depend on registry
        // iteration order or the sticky discount loses its anchor.
        std::sort(ctx.members.begin(), ctx.members.end(),
                  [](Member const& a, Member const& b2)
                  { return a.guid_low < b2.guid_low; });

        // Detect friendly carriers who are NOT bucketed bots — a human
        // teammate carrying the flag. Without this, PlanCtf would think
        // "no carrier" and send a pickup runner to an empty flag stand
        // while leaving the human FC unescorted.
        {
            std::vector<ObjectGuid> carriers = ref->bg.all_friendly_carriers;
            if (carriers.empty() && !ref->bg.friendly_flag_carrier.IsEmpty())
                carriers.push_back(ref->bg.friendly_flag_carrier);
            for (ObjectGuid const& cg : carriers)
            {
                bool is_member = false;
                for (auto const& m : ctx.members)
                    if (cg.GetCounter() == m.guid_low)
                    { is_member = true; break; }
                if (!is_member)
                {
                    ++ctx.external_carriers;
                    if (cg == ref->bg.friendly_flag_carrier)
                        ctx.scalar_carrier_is_external = true;
                }
            }
            ctx.friendly_carrier_x = ref->bg.friendly_carrier_x;
            ctx.friendly_carrier_y = ref->bg.friendly_carrier_y;
            ctx.friendly_carrier_z = ref->bg.friendly_carrier_z;
        }

        if (Map* bg_map = b.bg ? b.bg->FindBgMap() : nullptr)
        {
            auto const& go_store =
                bg_map->GetObjectsStore().Data.template FindContainer<GameObject>();
            for (auto const& [go_guid, go] : go_store)
            {
                if (!go || !go->isSpawned()) continue;
                if (go->GetGoType() != GAMEOBJECT_TYPE_FLAGDROP &&
                    go->GetGoType() != GAMEOBJECT_TYPE_NEW_FLAG_DROP)
                    continue;
                ctx.dropped_flags.push_back({go->GetPositionX(), go->GetPositionY(), go->GetPositionZ()});
            }
        }

        PlanTeam(ctx, now_ms);

        // Diagnostic dump line per team.
        int kinds[9] = {};
        for (auto const& m : ctx.members)
        {
            auto it = next_orders_.find(m.guid_low);
            if (it != next_orders_.end() && it->second.kind < 9)
                ++kinds[it->second.kind];
        }
        dump << "bg_type=" << b.type_id << " inst=" << b.instance_id
             << " team=" << uint32(b.team) << " bots=" << b.bots.size()
             << " atk=" << kinds[BgOrder::AttackNode]
             << " def=" << kinds[BgOrder::DefendNode]
             << " esc=" << kinds[BgOrder::EscortFC]
             << " hunt=" << kinds[BgOrder::HuntEFC]
             << " pickup=" << kinds[BgOrder::PickupFlag]
             << " carry=" << kinds[BgOrder::CarryHome]
             << " push=" << kinds[BgOrder::PushEndgame]
             << " drops=" << ctx.dropped_flags.size()
             << " delta=" << ctx.score_delta << "\n";
    }

    orders_ = std::move(next_orders_);
    next_orders_.clear();
    // Drop change-log signatures for teams that no longer exist (match
    // ended) so the map doesn't grow for the server's whole uptime.
    for (auto it = plan_sig_.begin(); it != plan_sig_.end();)
        it = buckets.count(it->first) ? std::next(it) : plan_sig_.erase(it);
    for (auto it = endgame_commit_.begin(); it != endgame_commit_.end();)
        it = buckets.count(it->first) ? std::next(it) : endgame_commit_.erase(it);
    for (auto it = mid_state_.begin(); it != mid_state_.end();)
        it = buckets.count(it->first) ? std::next(it) : mid_state_.erase(it);
    last_dump_ = dump.str();
    if (last_dump_.empty())
        last_dump_ = "no active bot battleground teams";
}

std::string BgTeamCoordinator::DebugDump() const
{
    return last_dump_.empty() ? std::string("coordinator has not planned yet")
                              : last_dump_;
}

} // namespace Playerbot
