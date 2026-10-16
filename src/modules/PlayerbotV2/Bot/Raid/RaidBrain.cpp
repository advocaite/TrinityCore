// RaidBrain implementation. See RaidBrain.h for the threading contract and
// RaidEncounters.h for the rule vocabulary.
//
// Movement (section "Movement") is a straight port of Karrz's ike3
// RaidBrain::DecideMove; the elections (section "Elections") port
// DecideRaidCast from MoltenCoreDungeonActions.cpp, minus interrupts (the
// PvE coordinator already runs an interrupt rotation).

#include "RaidBrain.h"
#include "DispelWorth.h"
#include "RaidBrainInternal.h"
#include "RaidEncounters.h"
#include "RaidGeometry.h"
#include "RaidReadiness.h"
#include "RaidHealStats.h"
#include "RaidIncoming.h"
#include "RaidManaLedger.h"
#include "RaidRoster.h"
#include "../../Fleet/BotPopulationManager.h"
#include "../../Fleet/OwnerRegistry.h"
#include "../BotRegistry.h"
#include "../ClassTables.h"
#include "../ThreatLimits.h"
#include "../Dungeon/PveGroupCoordinator.h"
#include "../../Group/GroupSnapshot.h"
#include "../../Fleet/BotIdentityRegistry.h"
#include "../../Services.h"
#include "../../Threading/SnapshotPublisher.h"
#include "../../Util/ConfigReader.h"

#include "Chat.h"
#include "Creature.h"
#include "DynamicObject.h"
#include "GameObject.h"
#include "GameTime.h"
#include "Group.h"
#include "GroupMgr.h"
#include "InstancePackets.h"
#include "InstanceScript.h"
#include "CreatureAI.h"
#include "Log.h"
#include "Map.h"
#include "MapManager.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "PathGenerator.h"
#include "PlayerbotMovement.h"
#include "Pet.h"
#include "Player.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellHistory.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "World.h"
#include "WorldSession.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <list>
#include <map>
#include <sstream>
#include <unordered_set>

namespace Playerbot::Raid {

namespace detail {

// A boss for the raid layer. This core strips CREATURE_FLAG_EXTRA_DUNGEON_BOSS from the templates at load (ObjectMgr
// flags_extra check), so Creature::IsDungeonBoss() is always false: use the world-boss flag, the vanilla "boss" rank
// (classification Obsolete) and the map's own boss list instead.
bool IsBoss(Creature const* c, RaidRules const* rules)
{
    if (!c)
        return false;
    if (c->isWorldBoss() || c->GetCreatureClassification() == CreatureClassifications::Obsolete)
        return true;
    return rules && std::find(rules->bosses.begin(), rules->bosses.end(), c->GetEntry()) != rules->bosses.end();
}

// Plan cadence. Hazards (Living Bomb, Inferno, Deep Breath) give a few
// seconds of warning; 300 ms keeps the reaction inside human range without
// running the path checks every world tick.
constexpr uint32_t kPlanIntervalMs = 300;

// ---------------------------------------------------------------------------
// Spells (vanilla rank chains; the emitter casts the highest rank known)
// ---------------------------------------------------------------------------
Ranks const kTaunt         = { 355 };                       // Warrior Taunt
Ranks const kGrowl         = { 6795 };                      // Druid Growl (bear)
Ranks const kDispelMagic   = { 527, 988 };                  // Priest
Ranks const kCleanse       = { 4987 };                      // Paladin: magic, poison, disease
Ranks const kPurify        = { 1152 };                      // Paladin: poison, disease
Ranks const kRemoveLesser  = { 475 };                       // Mage: curse
Ranks const kRemoveCurse   = { 2782 };                      // Druid: curse
Ranks const kAbolishPoison = { 2893 };                      // Druid
Ranks const kCurePoisonDru = { 8946 };                      // Druid
Ranks const kCurePoisonSha = { 526 };                       // Shaman
Ranks const kAbolishDisease= { 552 };                       // Priest
Ranks const kCureDisease   = { 528 };                       // Priest
Ranks const kCureDiseaseSha= { 2870 };                      // Shaman
Ranks const kPurge         = { 370, 8012 };                 // Shaman
Ranks const kBanish        = { 710, 18647 };                // Warlock
Ranks const kPolymorph     = { 118, 12824, 12825, 12826 };  // Mage
Ranks const kShackle       = { 9484, 9485, 10955 };         // Priest
Ranks const kHibernate     = { 2637, 18657, 18658 };        // Druid
Ranks const kHammerOfJustice = { 853, 5588, 5589, 10308 };  // Paladin stun
Ranks const kGouge         = { 1776, 1777, 8629, 11285, 11286 }; // Rogue incapacitate
Ranks const kBash          = { 5211, 6798, 8983 };          // Druid (bear) stun

// Majordomo Executus' adds: Magic Reflection / Damage Shield (mc/boss_majordomo_executus.cpp). Hitting them hurts.
constexpr uint32_t kMagicReflection = 20619;
constexpr uint32_t kDamageShield    = 21075;

std::vector<Ranks const*> DispelSpellsFor(DispelKind kind)
{
    switch (kind)
    {
        case DispelKind::Curse:   return { &kRemoveLesser, &kRemoveCurse };
        case DispelKind::Poison:  return { &kAbolishPoison, &kCurePoisonDru, &kCurePoisonSha, &kCleanse, &kPurify };
        case DispelKind::Disease: return { &kAbolishDisease, &kCureDisease, &kCureDiseaseSha, &kCleanse, &kPurify };
        case DispelKind::Magic:   break;
    }
    return { &kDispelMagic, &kCleanse };
}

Ranks const& CcSpellsFor(CcKind kind)
{
    switch (kind)
    {
        case CcKind::Banish:    return kBanish;
        case CcKind::Polymorph: return kPolymorph;
        case CcKind::Shackle:   return kShackle;
        case CcKind::Hibernate: return kHibernate;
    }
    return kBanish;
}

// The real dispel type of a debuff (data first; the rule's kind is the fallback).
DispelKind KindOf(Dispel const& rule)
{
    if (SpellInfo const* info = sSpellMgr->GetSpellInfo(rule.aura, DIFFICULTY_NONE))
        switch (info->Dispel)
        {
            case DISPEL_MAGIC:   return DispelKind::Magic;
            case DISPEL_CURSE:   return DispelKind::Curse;
            case DISPEL_DISEASE: return DispelKind::Disease;
            case DISPEL_POISON:  return DispelKind::Poison;
            default: break;
        }
    return rule.kind;
}

// Highest rank of the chain this player knows (0 = none).
uint32_t KnownRank(Player* p, Ranks const& ranks)
{
    for (auto it = ranks.rbegin(); it != ranks.rend(); ++it)
        if (p->HasSpell(*it))
            return *it;
    return 0;
}

// Cooldown, range and line of sight. Casting state is handled by the
// election (a busy caster is only a second choice) and by the consumer
// (it waits for the running cast to finish).
bool Ready(Player* p, uint32_t spellId, Unit* target, bool hostile)
{
    if (!spellId || !target)
        return false;
    SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
    if (!info)
        return false;
    if (!p->GetSpellHistory()->IsReady(info))
        return false;
    // A druid in Cat / Bear / Moonkin form cannot cast Remove Curse, Rebirth and the like (SPELL_FAILED_NOT_SHAPESHIFT, 10 in
    // the Gehennas pull): the job goes to a bot that can cast it now.
    if (p->GetShapeshiftForm() != FORM_NONE && info->CheckShapeshift(p->GetShapeshiftForm()) != SPELL_CAST_OK)
        return false;
    float const range = std::max(info->GetMaxRange(!hostile, p), 5.0f);
    if (target != p && (!p->IsWithinDistInMap(target, range) || !p->IsWithinLOSInMap(target)))
        return false;
    return true;
}

bool Busy(Player* p) { return p->IsNonMeleeSpellCast(false); }

// ---------------------------------------------------------------------------
// Members and enemies
// ---------------------------------------------------------------------------
bool Matches(Who who, Member const& m, Player* mainTank)
{
    switch (who)
    {
        case Who::Anyone:   return true;
        case Who::Tank:     return m.tank;
        case Who::MainTank: return m.p == mainTank;
        case Who::OffTank:  return m.tank && m.p != mainTank;
        case Who::NonTank:  return !m.tank;
        case Who::Melee:    return !m.tank && !m.healer && !m.ranged;
        case Who::Ranged:   return !m.tank && !m.healer && m.ranged;
        case Who::Healer:   return m.healer;
        case Who::ManaUser: return m.mana;
        case Who::Caster:   return !m.tank && !m.healer && m.ranged && m.p->GetClass() != CLASS_HUNTER;
        case Who::Physical: return !m.tank && !m.healer && (!m.ranged || m.p->GetClass() == CLASS_HUNTER);
        case Who::AoeShare: return !m.tank && !m.healer && m.ranged && m.p->GetClass() != CLASS_HUNTER && (m.p->GetGUID().GetCounter() & 1) == 0;
    }
    return false;
}

// Phase window of a rule (hpMax / hpMin in RaidEncounters.h): the enemy's health percent must lie inside it.
bool InPhase(Unit const* enemy, float hpMax, float hpMin)
{
    float const pct = enemy->GetHealthPct();
    return pct <= hpMax && pct >= hpMin;
}

float Dist2D(float ax, float ay, float bx, float by) { return std::sqrt((ax - bx) * (ax - bx) + (ay - by) * (ay - by)); }

bool InsideAny(std::vector<Circle> const& circles, float x, float y, float margin)
{
    for (Circle const& c : circles)
        if (Dist2D(x, y, c.x, c.y) < c.r + margin)
            return true;
    return false;
}

bool Controlled(Unit* u)
{
    if (u->IsPolymorphed())
        return true;
    for (Ranks const* ranks : { &kBanish, &kShackle, &kHibernate, &kPolymorph })
        for (uint32_t id : *ranks)
            if (u->HasAura(id))
                return true;
    return false;
}

// ---------------------------------------------------------------------------
// Movement (port of ike3 RaidBrain::DecideMove)
// ---------------------------------------------------------------------------

// Path queries in one plan pass, all bots together. A full 40-man pass with every bot spreading used to run ~1000 of them
// on the world thread. Routine probes (spread, role spots, stay near) stop at kPathProbeRoutine; urgent ones (leaving a hazard, a carrier
// run, scripted mechanics, a cone) have a budget of their own. They used to share one cap of 160 with the routine ones: the bots early in
// the pass spent 100 on spreading and left 60 for the escapes, about three probes for each of the 18 raiders inside Geddon's Inferno
// circle, so the ones late in the pass found no way out and stood in it (MC Geddon 2026-10-13 d202b196: six melee dead at 16-20 y, and
// the ranged starting out 0.6 s apart one pass at a time). The budget only bounds the cost: a bot whose probe was refused keeps its sticky
// point or retries next pass (300 ms).
constexpr int kUrgentProbeCap = 420;
constexpr int kPathProbeRoutine = 100;
int g_pathProbes = 0;
int g_urgentProbes = 0;

void ResetPathProbes() { g_pathProbes = 0; g_urgentProbes = 0; }

// Hostile creatures near the plan's raid that are not fighting yet, with their aggro reach against the raid (refreshed per
// plan, see SetIdleHostiles). A raid move never ends inside that reach: a bot leaving Smash or spreading used to run into
// the next pack and pull it (MC 2026-10-06: Lava Annihilator and Firelord joined a Molten Giant pull).
std::vector<Circle> g_idleHostiles;

void SetIdleHostiles(Plan const& plan, Player* ref)
{
    g_idleHostiles.clear();
    FindCreatureOptions options;
    options.IsAlive = FindCreatureAliveState::EffectivelyAlive;
    options.IsInCombat = false;
    std::list<Creature*> creatures;
    ref->GetCreatureListWithOptionsInGrid(creatures, 100.0f, options);
    for (Creature* creature : creatures)
    {
        if (!creature->IsHostileTo(ref) || creature->IsTrigger() || creature->IsCritter() ||
            std::find(plan.enemies.begin(), plan.enemies.end(), creature) != plan.enemies.end())
            continue;
        float const reach = creature->GetAttackDistance(ref);
        if (reach > 0.0f && g_idleHostiles.size() < 64)
            g_idleHostiles.push_back({ creature->GetPositionX(), creature->GetPositionY(), reach + 3.0f });
    }
}

// RaidRules::keepOut circles of the bosses in the fight (refreshed per plan): no raid move ends inside one. The candidate searches skip them
// (Escape, Clearest, Usable), so a bot leaving a breath corridor takes the next way out instead of the nearest one, which for Onyxia's
// north-south breath is the cave mouth and its whelps; ApplyKeepOut is the clamp for the moves that name a point (role spots, flanks).
std::vector<Circle> g_keepOut;

void SetKeepOut(Plan const& plan)
{
    g_keepOut.clear();
    for (KeepOut const& zone : plan.rules->keepOut)
        for (Unit* enemy : plan.enemies)
            if (enemy->GetEntry() == zone.enemy && enemy->IsAlive() && enemy->IsInCombat() && g_keepOut.size() < 16)
            {
                g_keepOut.push_back({ zone.x, zone.y, zone.radius });
                break;
            }
}

// Enemies already in the fight, with the reach of their melee. A ranged bot's free raid move (leave hazard, spread, stay near)
// never ends inside it and never runs through it: leaving Garr's Massive Eruption circle sent a mage across the Firesworn pack
// to the far side of the ring (mc_garr 2026-10-12: five casters and priests dead to Firesworn melee, last rule raid:move).
// A mob the bot already stands inside is skipped; nothing can be done about that one from here.
std::vector<Circle> g_engagedReach;
constexpr float kEngagedMeleeMargin = 6.0f;   // melee range 5 + a step

void SetEngagedReach(Plan const& plan)
{
    g_engagedReach.clear();
    for (Unit* enemy : plan.enemies)
        if (enemy && enemy->IsAlive() && !enemy->IsPlayer() && g_engagedReach.size() < 64)
            g_engagedReach.push_back({ enemy->GetPositionX(), enemy->GetPositionY(), enemy->GetCombatReach() + kEngagedMeleeMargin });
}

float SegmentDist2D(float ax, float ay, float bx, float by, float px, float py)
{
    float const dx = bx - ax, dy = by - ay;
    float const len2 = dx * dx + dy * dy;
    float const t = len2 > 0.0f ? std::clamp(((px - ax) * dx + (py - ay) * dy) / len2, 0.0f, 1.0f) : 0.0f;
    return Dist2D(px, py, ax + t * dx, ay + t * dy);
}

// True when the walk from the bot to (x, y) ends in or passes through the melee reach of an engaged enemy the bot is not
// already inside.
bool CrossesEngagedReach(Player* bot, float x, float y)
{
    float const bx = bot->GetPositionX(), by = bot->GetPositionY();
    for (Circle const& c : g_engagedReach)
    {
        if (Dist2D(bx, by, c.x, c.y) < c.r)
            continue;
        if (SegmentDist2D(bx, by, x, y, c.x, c.y) < c.r)
            return true;
    }
    return false;
}

// The fight a bot's free raid moves (leave hazard, cone, spread, stay near) must stay part of, set by DecideMove after the
// scripted mechanics had their turn (Heigan's dance, the stomach and Gothik's sides pick their own far points). Without it a
// bot leaving Smash or spreading took any walkable point: ledges and rocks off the side of the fight, with no sight of its
// target, or a path that climbed round a rock to get there (MC 2026-10-06, casters idle up on the rocks).
struct FightArea
{
    bool active = false;
    Unit* anchor = nullptr;   // the main tank (or the bot's target when there is none): the centre of the fight
    Unit* target = nullptr;   // what the bot fights (its victim, else the main tank's), or the main tank for a healer
    float targetRange = 0.0f; // ranged: stay within cast range of `target`; 0 for melee
};
FightArea g_fight;
bool g_carrierRun = false;   // set while a bomb carrier's escape point is searched (RaidBrain DecideMove)

constexpr float kFightRadius = 35.0f;   // from the main tank
constexpr float kRangedReach = 30.0f;   // casting / shooting range
constexpr float kDetourFactor = 1.4f;   // path length over straight-line distance
constexpr float kDetourSlack = 4.0f;

struct FightAreaScope
{
    FightAreaScope(Plan const& plan, Member const& self)
    {
        Player* bot = self.p;
        Unit* victim = bot->GetVictim();
        Unit* tankVictim = plan.mainTank && plan.mainTank != bot ? plan.mainTank->GetVictim() : nullptr;
        g_fight.anchor = plan.mainTank && plan.mainTank != bot ? static_cast<Unit*>(plan.mainTank) : (victim ? victim : tankVictim);
        if (self.healer)
            g_fight.target = plan.mainTank && plan.mainTank != bot ? static_cast<Unit*>(plan.mainTank) : nullptr;
        else
            g_fight.target = victim ? victim : tankVictim;
        g_fight.targetRange = (self.ranged || self.healer) && !self.tank ? kRangedReach : 0.0f;
        g_fight.active = g_fight.anchor || g_fight.target;
    }
    ~FightAreaScope() { g_fight = FightArea{}; }
};

// A distance limit that never traps a bot already beyond it: the point is fine within `limit`, or no further out than now.
bool WithinOrCloser(Player* bot, Unit* centre, float x, float y, float limit)
{
    float const there = Dist2D(x, y, centre->GetPositionX(), centre->GetPositionY());
    if (there <= limit)
        return true;
    return there <= Dist2D(bot->GetPositionX(), bot->GetPositionY(), centre->GetPositionX(), centre->GetPositionY()) + 1.0f;
}

// Cheap checks of a candidate point against the fight area (no path query).
bool InFightArea(Player* bot, float x, float y, float z, bool urgent)
{
    if (!g_fight.active)
        return true;
    // A carrier leaving the raid may go past the usual fight radius and out of casting range of its target: any way out comes first
    // (Living Bomb, MC 2026-10-12: "fight area" refused 12 of the probed points; a carrier on the far end of the 50 y ranged arc was 47 y
    // from the tank: b0a1c616, 9 more). The bomb lasts 8 s, so what a carrier can run is within 60 y of where it stands.
    if (g_fight.anchor && !WithinOrCloser(bot, g_fight.anchor, x, y, g_carrierRun ? kFightRadius + 60.0f : kFightRadius))
        return false;
    Unit* target = g_fight.target;
    if (!target || target == bot)
        return true;
    if (g_carrierRun)
        return true;
    if (g_fight.targetRange > 0.0f && !WithinOrCloser(bot, target, x, y, g_fight.targetRange + target->GetCombatReach()))
        return false;
    // A spot with no sight of the target is a spot the bot cannot fight from. Leaving a hazard goes first: any way out of it.
    if (!urgent && !bot->GetMap()->isInLineOfSight(bot->GetPhaseShift(), x, y, z + bot->GetCollisionHeight(),
            target->GetPositionX(), target->GetPositionY(), target->GetPositionZ() + target->GetCollisionHeight(),
            LINEOFSIGHT_ALL_CHECKS, VMAP::ModelIgnoreFlags::Nothing))
        return false;
    return true;
}

// A point the bot can actually stand on and walk to, outside every idle pack's aggro reach, and (inside DecideMove) part of
// the fight: near the main tank, in reach and sight of the bot's target, reached by a path without a detour.
// Why the last Usable() refused a point (Clearest tallies them for its "no reachable clear spot" log).
int g_usableWhy = 0;
constexpr char const* kUsableWhyName[] = { "ok", "idle pack", "engaged reach", "path probe cap", "no ground", "no LoS", "fight area", "no path", "detour" };

bool MagmaAt(Map* map, PhaseShift const& phase, float x, float y, float z, float& surfaceZ);

// `hazard`: the point is a way out of something that is killing the bot (Inferno's circle, a cone). It keeps the fight where it is only
// as far as the ground allows: a point in lava, with no ground, with no sight of the bot or no path is refused, and the fight-area and
// engaged-reach limits are not applied (MC Geddon 2026-10-13 650994ea: seven raiders inside Inferno's circle "found no way out" with the
// fight area and the engaged reach refusing 7-14 of their probes, and then stood in it).
bool Usable(Player* bot, float x, float y, float zHint, float& outZ, bool urgent, bool hazard)
{
    g_usableWhy = 0;
    if (!hazard && InsideAny(g_keepOut, x, y, 0.0f))   // a way out of a hazard may use the cave rim: a whelp pack can be survived, a breath cannot
        return g_usableWhy = 1, false;
    // Nor walk across one on the way (a flank re-post from one side of a cave mouth to the other). Open floor: the straight line stands
    // in for the path. A bot already inside is leaving and may cross its own circle.
    if (!hazard)
      for (Circle const& k : g_keepOut)
        if (Dist2D(bot->GetPositionX(), bot->GetPositionY(), k.x, k.y) >= k.r &&
            SegmentDist2D(bot->GetPositionX(), bot->GetPositionY(), x, y, k.x, k.y) < k.r)
            return g_usableWhy = 1, false;
    if (InsideAny(g_idleHostiles, x, y, 0.0f) && !InsideAny(g_idleHostiles, bot->GetPositionX(), bot->GetPositionY(), 0.0f))
        return g_usableWhy = 1, false;
    if (!hazard && g_fight.active && g_fight.targetRange > 0.0f && CrossesEngagedReach(bot, x, y))   // a ranged, non-tank bot
        return g_usableWhy = 2, false;
    if (urgent ? g_urgentProbes >= kUrgentProbeCap : g_pathProbes >= kPathProbeRoutine)
        return g_usableWhy = 3, false;
    Map* map = bot->GetMap();
    float const height = map->GetHeight(bot->GetPhaseShift(), x, y, zHint + 2.0f, true, 12.0f);
    if (height <= INVALID_HEIGHT + 1.0f || std::fabs(height - zHint) > 12.0f)
        return g_usableWhy = 4, false;
    // Lava is refused for every point, not only for ways out: the role spots, spread, taunt walk and cleaner reach used to be
    // able to pick one, and LeaveMagma then walked the bot out on the next plan, where the routine pick sent it back in.
    {
        float surface = 0.0f;
        if (MagmaAt(map, bot->GetPhaseShift(), x, y, height + 0.05f, surface))
            return g_usableWhy = 4, false;
    }
    if (!bot->IsWithinLOS(x, y, height + bot->GetCollisionHeight()))
        return g_usableWhy = 5, false;
    if (!hazard && !InFightArea(bot, x, y, height, urgent))
        return g_usableWhy = 6, false;
    ++(urgent ? g_urgentProbes : g_pathProbes);
    PathGenerator path(bot);
    if (!BotMovement::SehSafeCalculatePath(path, x, y, height))
        return g_usableWhy = 7, false;
    if (path.GetPathType() & (PATHFIND_NOPATH | PATHFIND_INCOMPLETE))
        return g_usableWhy = 7, false;
    if (g_fight.active && path.GetPath().size() >= 2)
    {
        // A walkable point reached by climbing round a rock is up on the rock: the path must be close to the straight line.
        float const dx = x - bot->GetPositionX(), dy = y - bot->GetPositionY(), dz = height - bot->GetPositionZ();
        float const straight = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (path.GetPathLength() > straight * kDetourFactor + kDetourSlack)
            return g_usableWhy = 8, false;
    }
    outZ = height;
    return true;
}

// The one clamp on every raid move (RaidRules::keepOut): a finished target inside a keep-out circle of a boss that is in the fight moves to the
// circle's rim, along the line from its centre. The ground there is looked up again; with none the move is dropped, never kept.
void ApplyKeepOut(Plan const& plan, Member const& self, std::vector<Circle> const& circles, RaidOrder& out)
{
    if (!out.move || plan.rules->keepOut.empty())
        return;
    // A way out of a hazard is not clamped: the cave rim is survivable, the breath is not.
    if (out.move_reason && (std::strcmp(out.move_reason, "leave hazard") == 0 || std::strcmp(out.move_reason, "leave cone") == 0))
        return;
    for (KeepOut const& zone : plan.rules->keepOut)
    {
        bool fighting = false;
        for (Unit* enemy : plan.enemies)
            if (enemy->GetEntry() == zone.enemy && enemy->IsAlive() && enemy->IsInCombat()) { fighting = true; break; }
        if (!fighting)
            continue;
        float const d = Dist2D(out.move_x, out.move_y, zone.x, zone.y);
        if (d >= zone.radius)
            continue;
        float ux = out.move_x - zone.x, uy = out.move_y - zone.y;
        if (d < 0.5f)
        {
            ux = self.p->GetPositionX() - zone.x; uy = self.p->GetPositionY() - zone.y;
            float const l = std::sqrt(ux * ux + uy * uy);
            ux = l > 0.5f ? ux / l : 1.0f; uy = l > 0.5f ? uy / l : 0.0f;
        }
        else { ux /= d; uy /= d; }
        float const nx = zone.x + ux * (zone.radius + 1.0f), ny = zone.y + uy * (zone.radius + 1.0f);
        float const nz = self.p->GetMap()->GetHeight(self.p->GetPhaseShift(), nx, ny, out.move_z + 2.0f, true, 12.0f);
        if (nz <= INVALID_HEIGHT + 1.0f || std::fabs(nz - out.move_z) > 12.0f)
        {
            out.move = false;
            return;
        }
        // The rim of a keep-out circle may lie inside a hazard the bot is leaving: no move then, not a walk back into it.
        if (InsideAny(circles, nx, ny, 1.0f))
        {
            out.move = false;
            return;
        }
        out.move_x = nx; out.move_y = ny; out.move_z = nz;
    }
}

bool SetMove(RaidOrder& out, float x, float y, float z, char const* reason)
{
    out.move = true;
    out.move_x = x; out.move_y = y; out.move_z = z;
    out.move_reason = reason;
    return true;
}

// `tankOf` (the raid, for a tank): a tank leaving a boss's circle goes out sideways to the raid, not straight back through it. The boss
// follows its tank when the circle ends, and the tank's way out is where the boss ends up: straight back put Geddon 19 y closer to the
// ranged block between two Infernos (MC Geddon 2026-10-12 b0a1c616: 14 bots in range of the second one).
std::string g_escapeWhy;   // why the last Escape found no way out, for the "[raid_circle] ... no way out" log

// Where the boss of the latest aura circle started the fight (its home point), and when that circle opened. A tank leaving the circle
// takes the boss with it, so it leaves toward this point once the boss has been pulled off it: left to the sideways exit alone the boss
// walked ~20 y toward the room's edge every Inferno (MC Geddon 2026-10-13 mc-n5: x 655 -> 565 over one run, then six raiders "found no
// way out" against the lava edges).
struct BossHome { float x = 0.0f, y = 0.0f; uint32_t at = 0; };
BossHome g_bossHome;
constexpr uint32_t kBossHomeMaxAgeMs = 25000;   // the circle, its tail and the walk back
constexpr float kBossHomeSlack = 6.0f;          // a boss within this of its home point is left where it is

bool Escape(Player* bot, std::vector<Circle> const& circles, RaidOrder& out, std::vector<Player*> const* tankOf = nullptr)
{
    int deepest = -1;
    float depth = -1.0f;
    for (size_t i = 0; i < circles.size(); ++i)
    {
        float const penetration = circles[i].r - Dist2D(bot->GetPositionX(), bot->GetPositionY(), circles[i].x, circles[i].y);
        if (penetration >= 0.0f && penetration > depth) { depth = penetration; deepest = int(i); }
    }
    if (deepest < 0)
        return false;

    Circle const& c = circles[deepest];
    float base = std::atan2(bot->GetPositionY() - c.y, bot->GetPositionX() - c.x); // straight away from the centre first
    if (tankOf && !tankOf->empty())
    {
        float rx = 0.0f, ry = 0.0f;
        for (Player* p : *tankOf)
        {
            rx += p->GetPositionX();
            ry += p->GetPositionY();
        }
        float const toRaid = std::atan2(ry / float(tankOf->size()) - c.y, rx / float(tankOf->size()) - c.x);
        // Perpendicular to the centre-to-raid line, on the side the tank already stands on.
        float const side = std::sin(base - toRaid) >= 0.0f ? 1.0f : -1.0f;
        float const away = std::cos(base - toRaid);   // 1 = the tank stands between the boss and the raid
        if (away > 0.3f)
        {
            // Sideways, but no more than 35 degrees off the tank's own way out: the exit point is picked on the ring round the boss, and a point 90
            // degrees round it from where the tank stands is a walk through the circle, past the boss (MC Geddon 2026-10-13 mc_geddon run 2: the
            // main tank started 19 y from Geddon, was sent to a point 36 y away on the far side, and died at tick four 3.3 y from him; the next
            // tank died 21 y out on the same kind of walk).
            constexpr float kMaxSideSwing = 0.61f;   // 35 degrees
            float const sideways = toRaid + side * float(M_PI) * 0.5f;
            float delta = sideways - base;
            while (delta > float(M_PI)) delta -= 2.0f * float(M_PI);
            while (delta < -float(M_PI)) delta += 2.0f * float(M_PI);
            base += std::clamp(delta, -kMaxSideSwing, kMaxSideSwing);
        }
        // Boss pulled off its home point: leave toward it, so the boss follows its tank back instead of further out. Only when that
        // walk does not cut through the boss: the straight line from the tank to the ring point on the home bearing must pass the centre
        // no closer than 70 % of the tank's own distance to it (the refusal rule of the n5 exit fix); otherwise the clamped base stands.
        if (g_bossHome.at && GameTime::GetGameTimeMS() - g_bossHome.at < kBossHomeMaxAgeMs)
        {
            float const homeDist = Dist2D(c.x, c.y, g_bossHome.x, g_bossHome.y);
            if (homeDist > kBossHomeSlack && homeDist < 80.0f)
            {
                float const homeAngle = std::atan2(g_bossHome.y - c.y, g_bossHome.x - c.x);
                float const tx = c.x + (c.r + 2.0f) * std::cos(homeAngle), ty = c.y + (c.r + 2.0f) * std::sin(homeAngle);
                float const bx = bot->GetPositionX(), by = bot->GetPositionY();
                float const sx = tx - bx, sy = ty - by;
                float const len2 = sx * sx + sy * sy;
                float const u = len2 > 0.01f ? std::clamp(((c.x - bx) * sx + (c.y - by) * sy) / len2, 0.0f, 1.0f) : 0.0f;
                float const closest = Dist2D(c.x, c.y, bx + sx * u, by + sy * u);
                if (closest >= 0.7f * Dist2D(c.x, c.y, bx, by))
                    base = homeAngle;
            }
        }
    }
    int pathChecks = 0, inOther = 0, why[9] = {};
    auto const tally = [&]()
    {
        std::ostringstream w;
        w << "in another circle " << inOther;
        for (int i = 1; i < 9; ++i)
            if (why[i])
                w << ", " << kUsableWhyName[i] << " " << why[i];
        g_escapeWhy = w.str();
    };
    // Rings out to 20 y past the deepest circle and every 15 degrees: with the live circle, the origin circle and the predicted one open
    // together (Geddon walking), the first rings are mostly inside another circle (6 of 7 probes in MC Geddon 650994ea).
    // First without the keep-out circles, then with them allowed: leaving a breath outranks staying out of the caves.
    for (int pass = 0; pass < 2; ++pass)
    {
    pathChecks = 0;
    for (float extra : { 2.0f, 5.0f, 9.0f, 14.0f, 20.0f })
        for (int k = 0; k < 24; ++k)
        {
            float const offset = k == 0 ? 0.0f : float((k + 1) / 2) * (float(M_PI) / 12.0f) * ((k % 2) ? 1.0f : -1.0f);
            float const px = c.x + (c.r + extra) * std::cos(base + offset);
            float const py = c.y + (c.r + extra) * std::sin(base + offset);
            if (InsideAny(circles, px, py, 1.0f))
            {
                ++inOther;
                continue;
            }
            if (pass == 0 && InsideAny(g_keepOut, px, py, 0.0f))
                continue;   // out of bounds (RaidRules::keepOut): the next way out, before a path probe is spent
            // A way out that runs through the middle of the circle is no way out: the walk passes the boss at under 70 percent of the bot's
            // own distance (a point on the far side of the ring is a walk across it, at the heaviest ticks).
            {
                float const sx = px - bot->GetPositionX(), sy = py - bot->GetPositionY();
                float const len2 = sx * sx + sy * sy;
                float const cx = c.x - bot->GetPositionX(), cy = c.y - bot->GetPositionY();
                float const t = len2 > 0.01f ? std::clamp((cx * sx + cy * sy) / len2, 0.0f, 1.0f) : 0.0f;
                float const nearest = std::sqrt((cx - t * sx) * (cx - t * sx) + (cy - t * sy) * (cy - t * sy));
                if (nearest < 0.7f * std::sqrt(cx * cx + cy * cy))
                    continue;
            }
            if (++pathChecks > 20)
                break;   // this pass's probe budget is spent
            float pz = 0.0f;
            if (!Usable(bot, px, py, bot->GetPositionZ(), pz, /*urgent*/ true, /*hazard*/ true))
            {
                ++why[g_usableWhy];
                continue;
            }
            return SetMove(out, px, py, pz, "leave hazard");
        }
    }
    tally();
    return false;
}

// Best nearby point that increases the distance to every raid member in `others`.
std::string g_clearestWhy;

bool Clearest(Player* bot, std::vector<Player*> const& others, float wanted, RaidOrder& out, char const* reason,
              std::vector<Circle> const& avoid, bool urgent)
{
    float nearest = 9999.0f;
    for (Player* p : others)
        nearest = std::min(nearest, Dist2D(bot->GetPositionX(), bot->GetPositionY(), p->GetPositionX(), p->GetPositionY()));

    // Candidate points on rings round the bot, scored first (cheap) and probed best first (a path query each). A carrier standing inside
    // the raid (Living Bomb, MC 2026-10-12: nearest raider 11-14 yd, 25-30 of the ring points "not better") needs the rings that reach past
    // the raid's edge, and probing in ring order spent the whole probe budget on the nearest, worst points.
    struct Candidate { float x, y, score; };
    std::vector<Candidate> candidates;
    float const improvement = urgent ? 2.0f : 4.0f;
    std::vector<float> rings = urgent ? std::vector<float>{ wanted * 0.7f, wanted, wanted * 1.4f, wanted * 1.9f }
                                      : std::vector<float>{ wanted, wanted * 1.4f };
    int const steps = urgent ? 24 : 16;
    int skippedNear = 0, skippedAvoid = 0, why[9] = {};
    for (float distance : rings)
        for (int i = 0; i < steps; ++i)
        {
            float const angle = float(i) * (2.0f * float(M_PI) / float(steps));
            float const px = bot->GetPositionX() + distance * std::cos(angle);
            float const py = bot->GetPositionY() + distance * std::sin(angle);
            float clear = 9999.0f;
            for (Player* p : others)
                clear = std::min(clear, Dist2D(px, py, p->GetPositionX(), p->GetPositionY()));
            if (clear < nearest + improvement)
            {
                ++skippedNear;
                continue; // must be a real improvement
            }
            if ((!avoid.empty() && InsideAny(avoid, px, py, 1.0f)) || InsideAny(g_keepOut, px, py, 0.0f))
            {
                ++skippedAvoid;
                continue;
            }
            // The cheap fight-area test first: a path probe is the scarce thing, and the best-scored points of a carrier run are the farthest
            // ones, which the fight area can refuse (MC Geddon 2026-10-13 09a8ff07: "fight area 14" - all 14 probes spent on refused points).
            if (g_carrierRun && !InFightArea(bot, px, py, bot->GetPositionZ(), urgent))
            {
                ++why[6];
                continue;
            }
            candidates.push_back({ px, py, std::min(clear, wanted + 12.0f) - 0.2f * distance });
        }
    std::stable_sort(candidates.begin(), candidates.end(), [](Candidate const& a, Candidate const& b) { return a.score > b.score; });

    int pathChecks = 0, snapChecks = 0, offMesh = 0;
    for (Candidate const& c : candidates)
    {
        if (pathChecks >= (urgent ? 14 : 12))
            break;
        float cx = c.x, cy = c.y;
        // A carrier's run spot is picked from the walkable navmesh first: ring points over a ledge, a pit or a wall had no ground and spent the
        // probe budget (Living Bomb, MC Geddon 2026-10-14 n38: "no reachable clear spot ... no ground 13-14" with raiders 1-19 y away). Each
        // point is moved to the nearest walkable poly within 3 y; no poly there, no probe. A moved point must still beat standing here.
        if (g_carrierRun)
        {
            if (++snapChecks > 60)
                break;
            Position snapped;
            if (!BotMovement::NearestNavPoint(bot, c.x, c.y, bot->GetPositionZ(), 3.0f, 6.0f, snapped))
            {
                ++offMesh;
                continue;
            }
            cx = snapped.GetPositionX();
            cy = snapped.GetPositionY();
            float clear = 9999.0f;
            for (Player* p : others)
                clear = std::min(clear, Dist2D(cx, cy, p->GetPositionX(), p->GetPositionY()));
            if (clear < nearest + improvement || (!avoid.empty() && InsideAny(avoid, cx, cy, 1.0f)))
            {
                ++offMesh;
                continue;
            }
        }
        ++pathChecks;
        float pz = 0.0f;
        if (!Usable(bot, cx, cy, bot->GetPositionZ(), pz, urgent))
        {
            ++why[g_usableWhy];
            continue;
        }
        return SetMove(out, cx, cy, pz, reason);
    }

    // Why nothing was found, for the carrier-run log: candidates no better than standing here, candidates inside a hazard circle, and
    // what refused each point that was probed.
    std::ostringstream w;
    w << "not better " << skippedNear << ", in hazard " << skippedAvoid;
    if (offMesh)
        w << ", off navmesh " << offMesh;
    for (int i = 1; i < 9; ++i)
        if (why[i])
            w << ", " << kUsableWhyName[i] << " " << why[i];
    g_clearestWhy = w.str();
    return false;
}

// Keeps a decision for a couple of seconds so bots do not jitter between equally good points.
// World thread only (Update), so no locking.
struct Sticky { float x, y, z; uint32_t at; char const* reason; };
std::unordered_map<uint64_t, Sticky> g_sticky;
std::unordered_map<uint64_t, uint32_t> g_lastSpread;
std::unordered_map<uint64_t, uint32_t> g_lastTankFace;
std::unordered_map<uint64_t, uint32_t> g_lastFaceAway;
std::unordered_map<uint64_t, uint32_t> g_lastRearStep;
std::unordered_map<uint64_t, uint32_t> g_lastCarrierLog;
std::unordered_map<uint64_t, uint32_t> g_lastBombLog;
std::unordered_map<uint64_t, uint32_t> g_lastStuckLog;

bool UseSticky(Player* bot, std::vector<Circle> const& circles, uint32_t now, RaidOrder& out)
{
    auto it = g_sticky.find(bot->GetGUID().GetCounter());
    if (it == g_sticky.end())
        return false;
    Sticky const& s = it->second;
    if (now - s.at > 2000 || Dist2D(bot->GetPositionX(), bot->GetPositionY(), s.x, s.y) < 2.0f || InsideAny(circles, s.x, s.y, 0.5f))
        return false;
    return SetMove(out, s.x, s.y, s.z, s.reason);
}

void Remember(Player* bot, RaidOrder const& d, uint32_t now)
{
    g_sticky[bot->GetGUID().GetCounter()] = { d.move_x, d.move_y, d.move_z, now, d.move_reason };
}

// Line hazard (AvoidBreath): the corridor is latched when the breath starts so it stays put while the enemy flies along it.
struct BreathLatch { float x = 0, y = 0, o = 0; uint32_t until = 0, lockout = 0, stateLogged = 0; };
std::unordered_map<uint64_t, BreathLatch> g_breath;
// The breath spell a fixed-path rule saw cast (AvoidBreath::fixedPath) and until when its corridor stays open.
struct BreathSpellLatch { uint32_t spell = 0, until = 0; };
std::unordered_map<uint64_t, BreathSpellLatch> g_breathSpell;

bool IsCasting(Unit* enemy, uint32_t spell)
{
    for (CurrentSpellTypes type : { CURRENT_GENERIC_SPELL, CURRENT_CHANNELED_SPELL })
    {
        Spell* cast = enemy->GetCurrentSpell(type);
        if (cast && cast->GetSpellInfo() && cast->GetSpellInfo()->Id == spell)
            return true;
    }
    return false;
}

// The breath spell the enemy is casting or carries right now, 0 for none (for the log line).
uint32_t CastingBreath(Unit* enemy, std::vector<uint32_t> const& spells)
{
    for (uint32_t spell : spells)
        if (spell && enemy->HasAura(spell))
            return spell;
    for (CurrentSpellTypes type : { CURRENT_GENERIC_SPELL, CURRENT_CHANNELED_SPELL })
        if (Spell* cast = enemy->GetCurrentSpell(type))
            if (cast->GetSpellInfo() && std::find(spells.begin(), spells.end(), cast->GetSpellInfo()->Id) != spells.end())
                return cast->GetSpellInfo()->Id;
    return 0;
}

bool BreathActive(Unit* enemy, std::vector<uint32_t> const& spells, bool windup)
{
    // Any of the enemy's breath spells: it casts the one of the waypoint it stands at, so checking only the first listed one saw one breath in eight.
    for (uint32_t spell : spells)
        if (spell && enemy->HasAura(spell))
            return true;
    for (CurrentSpellTypes type : { CURRENT_GENERIC_SPELL, CURRENT_CHANNELED_SPELL })
    {
        Spell* cast = enemy->GetCurrentSpell(type);
        if (cast && cast->GetSpellInfo() && std::find(spells.begin(), spells.end(), cast->GetSpellInfo()->Id) != spells.end())
            return true;
    }
    // The wind-up: airborne, standing still, no target (her script clears the target for the 5 s wait, then restores it on arrival).
    if (windup && enemy->IsFlying() && !enemy->isMoving() && enemy->GetTarget().IsEmpty())
        return true;
    // The breath pass itself: Onyxia flies it at run speed rate 3.0 (classic_boss_onyxia.cpp ONYXIA_BREATH_SPEED).
    return enemy->isMoving() && enemy->GetSpeedRate(MOVE_RUN) >= 2.5f;
}

bool BreathLine(Unit* enemy, std::vector<uint32_t> const& spells, bool windup, uint32_t now, float& x, float& y, float& o)
{
    bool const active = BreathActive(enemy, spells, windup);
    BreathLatch& latch = g_breath[enemy->GetGUID().GetCounter()];
    // While the breather is airborne, once a second: what the detection sees. A breath that killed the raid unlatched (onyxia_full n14, 36 deaths at
    // 195 s, one latch in the run) needs this to say which condition failed.
    if (enemy->IsFlying() && now - latch.stateLogged >= 1000)
    {
        latch.stateLogged = now;
        TC_LOG_INFO("playerbot.v2.raid", "[raid_breath_state] {} at {:.1f},{:.1f} facing {:.0f} flying {} moving {} target {} speed {:.1f} casting {} active {} lockout in {} ms latched for {} ms",
            enemy->GetName(), enemy->GetPositionX(), enemy->GetPositionY(), enemy->GetOrientation() * 180.0f / float(M_PI), enemy->IsFlying(), enemy->isMoving(),
            enemy->GetTarget().IsEmpty() ? "none" : "set", enemy->GetSpeedRate(MOVE_RUN), IsCasting(enemy, spells.empty() ? 0u : spells.front()), active,
            latch.lockout > now ? latch.lockout - now : 0u, latch.until > now ? latch.until - now : 0u);
    }
    if (active && now > latch.lockout)
    {
        latch.x = enemy->GetPositionX(); latch.y = enemy->GetPositionY(); latch.o = enemy->GetOrientation();
        latch.until = now + 10000;     // the wait (5 s) plus the flight
        TC_LOG_INFO("playerbot.v2.raid", "[raid_breath] {} breath corridor latched at {:.1f},{:.1f} facing {:.0f} deg (cast {} windup {} moving {} speed {:.1f})",
            enemy->GetName(), latch.x, latch.y, latch.o * 180.0f / float(M_PI), CastingBreath(enemy, spells), windup, enemy->isMoving(), enemy->GetSpeedRate(MOVE_RUN));
        latch.lockout = now + 9000;    // no re-latch at the end of the same pass (5 s wait + about 4 s flight; onyxia_full n8: a second breath 13 s after the first got no corridor under the old 14 s)
    }
    else if (active && now <= latch.until && Dist2D(enemy->GetPositionX(), enemy->GetPositionY(), latch.x, latch.y) < 3.0f)
        latch.o = enemy->GetOrientation(); // she turns to face the far waypoint just after the cast
    if (now > latch.until)
        return false;
    x = latch.x; y = latch.y; o = latch.o;
    return true;
}

// When an AvoidEnemyAura rule's aura or cast was last seen on an enemy, by (enemy guid << 32) ^ aura. World thread only.
std::unordered_map<uint64_t, uint32_t> g_auraCircleSeen;
constexpr uint32_t kAuraCircleTailMs = 2500;
// Where the enemy stood when the circle opened, and whether the drift past it has been logged. A boss that walks during the spell (MC Geddon
// 2026-10-13 d5753b04: the Inferno circle opened at 653.8,-821.7 and the damage came from 645.1,-809.9, 13.7 y away, on bots that had just
// reached the edge of the first circle) keeps both circles open: the bots leave the place it was and the place it is.
struct AuraCircleOrigin { float x, y; bool drift; uint32_t openedMs; };
uint32_t g_auraCircleOpenedMs = 0;                              // when the latest aura circle opened (diagnostic: how fast the bots start leaving)
std::unordered_map<uint64_t, uint32_t> g_auraCircleLeaveLogged;  // bot -> g_auraCircleOpenedMs it was logged for
std::unordered_map<uint64_t, AuraCircleOrigin> g_auraCircleOrigin;

uint32_t g_breathEnemy = 0, g_breathCast = 0;   // the fixed-path breath whose corridor HazardsFor just opened for this member (0 for none)

// Every avoid-circle that applies to this member right now.
std::vector<Circle> HazardsFor(Plan const& plan, Member const& self, uint32_t now)
{
    RaidRules const& rules = *plan.rules;
    Player* bot = self.p;
    std::vector<Circle> circles;
    g_breathCast = 0;
    for (AvoidEnemyAura const& rule : rules.avoidEnemyAura)
        if (Matches(rule.who, self, plan.mainTank))
            for (Unit* enemy : plan.enemies)
            {
                if (enemy->GetEntry() != rule.enemy)
                    continue;
                // The aura, or the cast that applies it: the circle opens when the boss starts the spell, not a plan
                // interval after it landed (MC Geddon 2026-10-11: 35 of 36 deaths in one Inferno, bots still inside).
                // It stays open kAuraCircleTailMs after both end: the last, heaviest tick of a periodic spell (Inferno: 5000) lands after the
                // boss's aura is gone, and a bot walking back at the aura's end took it (MC Geddon 2026-10-12).
                uint64_t const key = (enemy->GetGUID().GetCounter() << 32) ^ rule.aura;
                if (enemy->HasAura(rule.aura) || IsCasting(enemy, rule.aura))
                {
                    // The circle opens: who stands inside it right now, for the next wipe's analysis (MC Geddon 2026-10-13: only the death
                    // distances were known, not where the raid stood when the cast began). One line per cast.
                    auto const prev = g_auraCircleSeen.find(key);
                    if (prev == g_auraCircleSeen.end() || now - prev->second > kAuraCircleTailMs)
                    {
                        uint32_t inside = 0, ranged = 0, tanks = 0;
                        float nearest = 9999.0f;
                        for (Member const& m : plan.members)
                        {
                            float const d = Dist2D(m.p->GetPositionX(), m.p->GetPositionY(), enemy->GetPositionX(), enemy->GetPositionY());
                            nearest = std::min(nearest, d);
                            if (d <= rule.radius)
                            {
                                ++inside;
                                ranged += (m.ranged || m.healer) ? 1u : 0u;
                                tanks += m.tank ? 1u : 0u;
                            }
                        }
                        TC_LOG_INFO("playerbot.v2.raid", "[raid_circle] {} ({}) starts aura {}: circle {:.0f} y at {:.1f},{:.1f}; {} of {} members inside ({} ranged/healers, {} tanks), nearest {:.1f} y",
                            enemy->GetName(), enemy->GetEntry(), rule.aura, rule.radius, enemy->GetPositionX(), enemy->GetPositionY(), inside,
                            plan.members.size(), ranged, tanks, nearest);
                        if (Creature const* boss = enemy->ToCreature())
                        {
                            Position const& home = boss->GetHomePosition();
                            g_bossHome = { home.GetPositionX(), home.GetPositionY(), GameTime::GetGameTimeMS() };
                        }
                        g_auraCircleOrigin[key] = { enemy->GetPositionX(), enemy->GetPositionY(), false, now ? now : 1u };
                        g_auraCircleOpenedMs = now ? now : 1u;
                    }
                    g_auraCircleSeen[key] = now;
                }
                auto const seen = g_auraCircleSeen.find(key);
                if (seen != g_auraCircleSeen.end() && now - seen->second <= kAuraCircleTailMs)
                {
                    circles.push_back({ enemy->GetPositionX(), enemy->GetPositionY(), rule.radius });
                    auto origin = g_auraCircleOrigin.find(key);
                    if (origin != g_auraCircleOrigin.end())
                    {
                        float const drift = Dist2D(origin->second.x, origin->second.y, enemy->GetPositionX(), enemy->GetPositionY());
                        if (drift > 2.0f && circles.size() < 64)
                            circles.push_back({ origin->second.x, origin->second.y, rule.radius });
                        // Where the walk ends: the drift so far, carried on to the end of the aura. Geddon walked ~15 y in one Inferno (MC
                        // 2026-10-13 2318f9d0): the ranged block stood outside the circle where he began, was run over at the seventh second and
                        // started out 0.3 s before the last tick. Capped, so one stray step does not throw a circle across the room.
                        if (drift > 2.0f && now > origin->second.openedMs + 500u && circles.size() < 64)
                            if (Aura const* aura = enemy->GetAura(rule.aura); aura && aura->GetDuration() > 0)
                            {
                                float const elapsed = float(now - origin->second.openedMs);
                                float const ahead = std::min(25.0f, drift / elapsed * float(std::min(aura->GetDuration(), 10000)));
                                float const dx = enemy->GetPositionX() - origin->second.x, dy = enemy->GetPositionY() - origin->second.y;
                                circles.push_back({ enemy->GetPositionX() + dx / drift * ahead, enemy->GetPositionY() + dy / drift * ahead, rule.radius });
                            }
                        if (drift > 3.0f && !origin->second.drift)
                        {
                            origin->second.drift = true;
                            // What moves it: moving now, the root state, a root aura on it, and the generator on top (1 idle, 4 chase, 5 follow...).
                            // When it moved: ms since the circle opened and what is left of the aura (-1: the aura is gone and this is the
                            // tail). Geddon's script roots him for the eight ticks and lets go at the last one while the 10 s aura still has
                            // time on it, so a walk 7 s in with the root state clear is the chase starting again, not a root that failed.
                            Aura const* const live = enemy->GetAura(rule.aura);
                            TC_LOG_INFO("playerbot.v2.raid", "[raid_circle] {} ({}) moved {:.1f} y after aura {} opened (now {:.1f},{:.1f}), {} ms in, aura left {} ms; both circles stay open; moving {}, root state {}, root aura {}, generator {}, script {}",
                                enemy->GetName(), enemy->GetEntry(), drift, rule.aura, enemy->GetPositionX(), enemy->GetPositionY(),
                                now - origin->second.openedMs, live ? live->GetDuration() : -1,
                                enemy->isMoving() ? 1 : 0, enemy->HasUnitState(UNIT_STATE_ROOT) ? 1 : 0, enemy->HasAuraType(SPELL_AURA_MOD_ROOT) ? 1 : 0,
                                int(enemy->GetMotionMaster()->GetCurrentMovementGeneratorType()),
                                enemy->ToCreature() ? enemy->ToCreature()->GetScriptName() : std::string());
                        }
                    }
                }
            }
    for (AvoidCarrier const& rule : rules.avoidCarrier)
        if (Matches(rule.who, self, plan.mainTank) && !bot->HasAura(rule.aura))
            for (Member const& m : plan.members)
                if (m.p != bot && m.p->HasAura(rule.aura))
                    circles.push_back({ m.p->GetPositionX(), m.p->GetPositionY(), rule.radius });
    for (AvoidGroundEffect const& rule : rules.avoidGroundEffect)
        if (Matches(rule.who, self, plan.mainTank))
            for (Unit* enemy : plan.enemies)
                if (enemy->GetEntry() == rule.enemy)
                    for (DynamicObject* object : enemy->GetDynObjects(rule.spell))
                        circles.push_back({ object->GetPositionX(), object->GetPositionY(), object->GetRadius() + rule.margin });
    for (AvoidGameObject const& rule : rules.avoidGameObject)
        if (Matches(rule.who, self, plan.mainTank))
        {
            std::list<GameObject*> objects;
            bot->GetGameObjectListWithEntryInGrid(objects, rule.entry, 40.0f);
            for (GameObject* object : objects)
                if (object->isSpawned() && circles.size() < 64)
                    circles.push_back({ object->GetPositionX(), object->GetPositionY(), rule.radius });
        }
    for (AvoidCreature const& rule : rules.avoidCreature)
        if (Matches(rule.who, self, plan.mainTank))
        {
            std::list<Creature*> creatures;
            bot->GetCreatureListWithEntryInGrid(creatures, rule.entry, 40.0f);
            for (Creature* creature : creatures)
                if (creature->IsAlive() != rule.corpse && circles.size() < 64)
                    circles.push_back({ creature->GetPositionX(), creature->GetPositionY(), rule.radius });
        }
    for (AvoidEnemy const& rule : rules.avoidEnemy)
        if (Matches(rule.who, self, plan.mainTank))
            for (Unit* enemy : plan.enemies)
                // Spacing from an enemy that hits someone else. One that is hitting this bot cannot be outrun: running
                // only drags it through the raid and into the next pack (MC 2026-10-06, loose Molten Giants chased their
                // casters across the room). The bot stays for a tank's taunt instead.
                if (enemy->GetEntry() == rule.enemy && enemy->GetVictim() != bot && InPhase(enemy, rule.hpMax, rule.hpMin))
                    circles.push_back({ enemy->GetPositionX(), enemy->GetPositionY(), rule.radius });
    if (!rules.avoidBreath.empty())
    {
        std::vector<uint32_t> breathAdded; // one corridor per enemy, however many breath spells are listed for it
        for (AvoidBreath const& rule : rules.avoidBreath)
        {
            if (!Matches(rule.who, self, plan.mainTank))
                continue;
            if (std::find(breathAdded.begin(), breathAdded.end(), rule.enemy) != breathAdded.end())
                continue;
            // The breathing enemy may be airborne and missing from the combat lists: fall back to a map search.
            Unit* boss = nullptr;
            for (Unit* enemy : plan.enemies)
                if (enemy->GetEntry() == rule.enemy)
                    boss = enemy;
            if (!boss)
                if (Creature* creature = bot->FindNearestCreature(rule.enemy, 200.0f, true))
                    if (creature->IsInCombat())
                        boss = creature;
            float lx, ly, lo;
            std::vector<uint32_t> spells;
            for (AvoidBreath const& other : rules.avoidBreath)
                if (other.enemy == rule.enemy)
                    spells.push_back(other.castSpell);
            if (rule.fixedPath)
            {
                // The corridor of the breath spell the enemy is casting, from the spell's own end points. Latched by spell id for the cast
                // plus the chained triggers (5 s cast, then 250 ms per step), so it holds while she flies on.
                if (!boss)
                    continue;
                BreathSpellLatch& latch = g_breathSpell[boss->GetGUID().GetCounter()];
                if (uint32_t const cast = CastingBreath(boss, spells))
                {
                    if (latch.spell != cast || now > latch.until)
                        TC_LOG_INFO("playerbot.v2.raid", "[raid_breath] {} breath spell {} corridor opened", boss->GetName(), cast);
                    latch.spell = cast;
                    latch.until = now + 11000;
                }
                if (!latch.spell || now > latch.until)
                    continue;
                for (AvoidBreath const& path : rules.avoidBreath)
                    if (path.enemy == rule.enemy && path.fixedPath && path.castSpell == latch.spell && Matches(path.who, self, plan.mainTank))
                    {
                        breathAdded.push_back(rule.enemy);
                        g_breathEnemy = rule.enemy;
                        g_breathCast = path.castSpell;
                        float const dx = path.bx - path.ax, dy = path.by - path.ay;
                        float const len = std::sqrt(dx * dx + dy * dy);
                        float const step = std::max(path.halfWidth * 0.8f, 2.0f);
                        if (len < 1.0f)
                            break;
                        // from 10 yards before the first point to 10 past the last one
                        for (float d = -10.0f; d <= len + 10.0f + step && circles.size() < 64; d += step)
                            circles.push_back({ path.ax + dx / len * d, path.ay + dy / len * d, path.halfWidth });
                        break;
                    }
                continue;
            }
            if (!boss || !BreathLine(boss, spells, rule.windup, now, lx, ly, lo))
                continue;
            breathAdded.push_back(rule.enemy);
            // Rectangle -> chain of circles of radius halfWidth, spaced 0.8 * halfWidth along the centre line, starting 10 yards behind.
            float const cs = std::cos(lo), sn = std::sin(lo);
            float const step = std::max(rule.halfWidth * 0.8f, 2.0f);
            float const total = rule.length + 10.0f;
            for (float d = 0.0f; d <= total + step && circles.size() < 64; d += step)
            {
                float const along = d - 10.0f;
                circles.push_back({ lx + along * cs, ly + along * sn, rule.halfWidth });
            }
        }
    }
    return circles;
}

// The nearest stayNear enemy for this bot (nullptr when no rule applies), and its rule radius.
Unit* StayNearEnemy(Plan const& plan, Member const& self, float& radius)
{
    Player* bot = self.p;
    Unit* nearest = nullptr;
    float nearestDist = 0.0f;
    for (StayNear const& rule : plan.rules->stayNear)
    {
        if (!Matches(rule.who, self, plan.mainTank))
            continue;
        for (Unit* enemy : plan.enemies)
        {
            if (enemy->GetEntry() != rule.enemy || !InPhase(enemy, rule.hpMax, rule.hpMin))
                continue;
            float const d = Dist2D(bot->GetPositionX(), bot->GetPositionY(), enemy->GetPositionX(), enemy->GetPositionY());
            if (!nearest || d < nearestDist)
            {
                nearest = enemy;
                nearestDist = d;
                radius = rule.radius;
            }
        }
    }
    return nearest;
}

// Movement context for the generic movement rules (Brain::MoveContextFor), built for every planned bot.
Brain::MoveContext BuildMoveContext(Plan const& plan, Member const& self, std::vector<Circle> const& circles)
{
    Brain::MoveContext ctx;
    float const bx = self.p->GetPositionX(), by = self.p->GetPositionY();
    ctx.circles.reserve(circles.size());
    for (Circle const& c : circles)
        ctx.circles.push_back({ c.x, c.y, c.r });

    // Frontal cones (breaths, cleaves, tail sweeps) as a short chain of circles inscribed in the sector along its axis, so the
    // generic approach and escape steps steer around them. Outermost first (d + r = radius), each next one touching the one
    // before; every circle keeps its near edge >= kApexGap from the enemy so the enemy itself never reads as standing in a
    // hazard (that would hold melee off a boss they are meant to hit from the side).
    constexpr float kApexGap = 4.0f;
    for (AvoidFrontal const& rule : plan.rules->avoidFrontal)
    {
        if (!Matches(rule.who, self, plan.mainTank))
            continue;
        float const s = std::sin(std::min(rule.halfAngleDeg, 89.0f) * (float(M_PI) / 180.0f));
        for (Unit* enemy : plan.enemies)
        {
            // A flying enemy has no ground cone (Onyxia's phase 2: she circles the cave and turns every waypoint).
            if (enemy->GetEntry() != rule.enemy || !enemy->IsAlive() || enemy->IsFlying() || !InPhase(enemy, rule.hpMax, rule.hpMin))
                continue;
            float const facing = enemy->GetOrientation() + rule.facingOffsetDeg * (float(M_PI) / 180.0f);
            float const cx = std::cos(facing), cy = std::sin(facing);
            float d = rule.radius / (1.0f + s);
            float r = d * s;
            if (d - r < kApexGap)
            {
                // A wide or short cone: one circle spanning kApexGap..radius on the axis.
                if (rule.radius <= 2.0f * kApexGap)
                    continue;
                d = (rule.radius + kApexGap) * 0.5f;
                r = (rule.radius - kApexGap) * 0.5f;
            }
            for (int k = 0; k < 3 && r >= 1.5f && d - r >= kApexGap; ++k)
            {
                ctx.circles.push_back({ enemy->GetPositionX() + cx * d, enemy->GetPositionY() + cy * d, r });
                d = d * (1.0f - s) / (1.0f + s);
                r = d * s;
            }
        }
    }

    // Flank stands (Onyxia): a melee bot's own approach to the boss (backstab behind the target, the nearest contact point, 1 y behind her) is
    // what walked it into the tail sweep cone again and again (onyxia_full n8: 26-33 "leave cone" steps per melee bot, 128 flank steps). The
    // cone circles above keep 4 y clear of the boss, so the apex behind her was unguarded. Keep-out circles over the rear half and the front
    // arc leave only the flank wedge open to the generic approach.
    for (StandFlank const& rule : plan.rules->standFlank)
    {
        if (!Matches(rule.who, self, plan.mainTank) || self.tank)
            continue;
        for (Unit* enemy : plan.enemies)
        {
            if (enemy->GetEntry() != rule.enemy || !enemy->IsAlive() || enemy->IsFlying() || !InPhase(enemy, rule.hpMax, rule.hpMin))
                continue;
            float const eo = enemy->GetOrientation(), ex = enemy->GetPositionX(), ey = enemy->GetPositionY();
            // Rear: three circles across the back half; front: one in line with her facing. Radius 5 at 6.5 y.
            for (float offDeg : { 180.0f, 140.0f, -140.0f })
            {
                float const a = eo + offDeg * (float(M_PI) / 180.0f);
                ctx.circles.push_back({ ex + 6.5f * std::cos(a), ey + 6.5f * std::sin(a), 5.0f });
            }
            ctx.circles.push_back({ ex + 6.5f * std::cos(eo), ey + 6.5f * std::sin(eo), 4.5f });
        }
    }

    // The keep-out zones (Onyxia's cave mouths) too: the bot's own move pickers (melee and healer gap-close, the LoS and cone steps,
    // spacing, the formation slot) read only these circles, so without them a melee bot chasing a fresh whelp walked into the cave.
    for (Circle const& k : g_keepOut)
        ctx.circles.push_back({ k.x, k.y, k.r });
    std::sort(ctx.circles.begin(), ctx.circles.end(), [&](Brain::MoveContext::Circle const& a, Brain::MoveContext::Circle const& c)
        { return Dist2D(bx, by, a.x, a.y) - a.r < Dist2D(bx, by, c.x, c.y) - c.r; });
    float radius = 0.0f;
    if (Unit* enemy = StayNearEnemy(plan, self, radius))
    {
        ctx.stay_near = true;
        ctx.stay_x = enemy->GetPositionX();
        ctx.stay_y = enemy->GetPositionY();
        ctx.stay_radius = radius;
    }
    return ctx;
}

// The axis of a FaceAway fight, locked when it first becomes known: the bearing from the boss to the tank's spot, i.e. from the raid's
// centre through the boss (Magmadar 2026-10-10: re-reading the raid's centre every few seconds made the tank step round the boss 25
// times, and each step turned the boss and sent the whole raid to a new "behind" spot, 1460 moves in one fight). It holds while the
// boss stays within kAxisDrift of where it was locked and is used for a replacement tank too, so the formation outlives the main tank.
struct FaceAxisLock { float bossX = 0, bossY = 0, bearing = 0; uint32_t seenMs = 0; };
std::unordered_map<uint64_t, FaceAxisLock> g_faceAxis;
constexpr float kAxisDrift = 8.0f;
constexpr uint32_t kAxisStaleMs = 30000;

bool FaceAxisFor(Plan const& plan, Unit* enemy, uint32_t now, float& axis, float fixedBearingDeg = kNoBearing)
{
    if (fixedBearingDeg != kNoBearing)
    {
        axis = fixedBearingDeg * (float(M_PI) / 180.0f);
        return true;
    }
    uint64_t const key = (plan.groupLow << 32) ^ enemy->GetEntry();
    float const ex = enemy->GetPositionX(), ey = enemy->GetPositionY();
    FaceAxisLock& lock = g_faceAxis[key];
    if (lock.seenMs && now - lock.seenMs < kAxisStaleMs && geo::AxisHolds(lock.bossX, lock.bossY, ex, ey, kAxisDrift))
    {
        lock.seenMs = now;
        axis = lock.bearing;
        return true;
    }
    float sx = 0.0f, sy = 0.0f;
    int n = 0;
    for (Member const& m : plan.members)
    {
        if (m.tank || Dist2D(m.p->GetPositionX(), m.p->GetPositionY(), ex, ey) > 60.0f)
            continue;
        sx += m.p->GetPositionX(); sy += m.p->GetPositionY();
        ++n;
    }
    if (n < 4)
        return false;
    lock = { ex, ey, geo::Bearing(sx / float(n), sy / float(n), ex, ey), now ? now : 1u };
    axis = lock.bearing;
    TC_LOG_INFO("playerbot.v2.raid", "[raid_face] axis for {} locked: raid centre {:.0f} yd out, {} raiders, bearing {:.0f} deg", enemy->GetName(),
        Dist2D(sx / float(n), sy / float(n), ex, ey), n, axis * 180.0f / float(M_PI));
    return true;
}

// A bot standing in lava or slime leaves it for the nearest dry navmesh point. The Ragnaros fight (live 2026-10-12) had bots standing in
// the lava round the platform and not moving out; the log line says whether the core even applies the liquid to them (z against the
// surface), the move takes them out. "In" is the core's own contact test (MAP_LIQUID_STATUS_IN_CONTACT: in, under, or on the surface within
// a tenth of a yard), the one that starts the lava damage. A floor a few tenths above the surface is not lava: the Ragnaros platform is a
// model floor lying just over a magma sheet (2026-10-13: 169 "stands in magma" lines for bots at z -228.6 on a surface of -229.0, none of
// them hurt, each sent to another point of the same floor), and treating it as lava kept the whole raid walking.
std::unordered_map<uint64_t, uint32_t> g_lastLavaLog;

bool MagmaAt(Map* map, PhaseShift const& phase, float x, float y, float z, float& surfaceZ)
{
    LiquidData ld;
    ZLiquidStatus const st = map->GetLiquidStatus(phase, x, y, z, map_liquidHeaderTypeFlags::Magma | map_liquidHeaderTypeFlags::Slime, &ld);
    if (!(st & MAP_LIQUID_STATUS_IN_CONTACT))
        return false;
    surfaceZ = ld.level;
    return true;
}

bool InMagma(Player* bot, float& surfaceZ)
{
    return MagmaAt(bot->GetMap(), bot->GetPhaseShift(), bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), surfaceZ);
}

// The nearest navmesh footing that is not itself in magma: rings of candidates round the bot, each snapped to the mesh and tested with the
// same contact check as the bot's own spot, so a leave-lava move never aims at another lava point.
bool ProbeDryFooting(Player* bot, Position& dry)
{
    constexpr int kBearings = 16;
    constexpr float kRings[] = { 2.0f, 4.0f, 7.0f, 10.0f, 14.0f, 19.0f, 25.0f };
    float const bx = bot->GetPositionX(), by = bot->GetPositionY(), bz = bot->GetPositionZ();
    for (float ring : kRings)
    {
        float bestD = 1e9f;
        bool found = false;
        for (int k = 0; k < kBearings; ++k)
        {
            float const a = (2.0f * float(M_PI) * float(k)) / float(kBearings);
            Position p;
            if (!BotMovement::NearestNavPoint(bot, bx + std::cos(a) * ring, by + std::sin(a) * ring, bz, 2.0f, 6.0f, p))
                continue;
            float surface = 0.0f;
            if (MagmaAt(bot->GetMap(), bot->GetPhaseShift(), p.GetPositionX(), p.GetPositionY(), p.GetPositionZ() + 0.05f, surface))
                continue;
            float const d = Dist2D(bx, by, p.GetPositionX(), p.GetPositionY());
            if (d < bestD)
            {
                bestD = d;
                dry = p;
                found = true;
            }
        }
        if (found)
            return true;
    }
    return false;
}

// The probe is up to 112 navmesh queries, and LeaveMagma runs it for every bot in lava on every 300 ms plan. A bot that has not moved
// more than 3 y keeps the answer for 1.5 s.
bool NearestDryFooting(Player* bot, Position& dry)
{
    struct Cached { uint32_t at = 0; float x = 0.0f, y = 0.0f; Position dry; bool ok = false; };
    static std::unordered_map<uint64_t, Cached> cache;
    uint32_t const now = GameTime::GetGameTimeMS();
    Cached& c = cache[bot->GetGUID().GetCounter()];
    if (c.at && now - c.at < 1500 && Dist2D(c.x, c.y, bot->GetPositionX(), bot->GetPositionY()) < 3.0f)
    {
        if (c.ok)
            dry = c.dry;
        return c.ok;
    }
    c.at = now;
    c.x = bot->GetPositionX();
    c.y = bot->GetPositionY();
    c.ok = ProbeDryFooting(bot, c.dry);
    if (c.ok)
        dry = c.dry;
    if (cache.size() > 512)
        for (auto it = cache.begin(); it != cache.end();)
            it = (now - it->second.at > 10000) ? cache.erase(it) : std::next(it);
    return c.ok;
}

bool LeaveMagma(Player* bot, uint32_t now, RaidOrder& out)
{
    float surface = 0.0f;
    if (!InMagma(bot, surface))
        return false;
    uint32_t& at = g_lastLavaLog[bot->GetGUID().GetCounter()];
    bool const log = now - at >= 5000;
    if (log)
        at = now;
    Position dry;
    if (!NearestDryFooting(bot, dry))
    {
        if (log)
            TC_LOG_INFO("playerbot.v2.raid", "[raid_lava] {} stands in magma at ({:.1f},{:.1f},{:.1f}), surface z {:.1f}, hp {}%: no navmesh point within 25 y",
                bot->GetName(), bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), surface, uint32(bot->GetHealthPct()));
        return false;
    }
    if (log)
        TC_LOG_INFO("playerbot.v2.raid", "[raid_lava] {} stands in magma at ({:.1f},{:.1f},{:.1f}), surface z {:.1f}, hp {}%: leaves for ({:.1f},{:.1f},{:.1f}); "
            "moving {}, casting {}, cast keeps it standing {}, cannot move (root/stun) {}, generator {}",
            bot->GetName(), bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), surface, uint32(bot->GetHealthPct()),
            dry.GetPositionX(), dry.GetPositionY(), dry.GetPositionZ(),
            bot->isMoving(), bot->IsNonMeleeSpellCast(false), bot->IsMovementPreventedByCasting(),
            bot->HasUnitState(UNIT_STATE_NOT_MOVE), uint32(bot->GetMotionMaster()->GetCurrentMovementGeneratorType()));
    return SetMove(out, dry.GetPositionX(), dry.GetPositionY(), dry.GetPositionZ(), "leave lava");
}

void DecideMove(Plan const& plan, Member const& self, uint32_t now, std::vector<Circle> const& circles, RaidOrder& out)
{
    // 0. Scripted mechanics (Heigan's dance, Sapphiron's ice blocks) own the bot's movement while they run.
    if (MechanicsMove(plan, self, now, out))
        return;
    FightAreaScope const fightArea(plan, self);

    RaidRules const& rules = *plan.rules;
    Player* bot = self.p;
    std::vector<Player*> others;
    for (Member const& m : plan.members)
        if (m.p != bot)
            others.push_back(m.p);
    if (others.empty())
        return;

    // Standing in lava or slime comes before every other move.
    if (LeaveMagma(bot, now, out))
        return;

    // 1. Carrier run (Living Bomb style): leave the group until clear of everyone.
    for (CarrierRun const& rule : rules.carrierRun)
        if (bot->HasAura(rule.aura) && Matches(rule.who, self, plan.mainTank))
        {
            float nearest = 9999.0f;
            int within = 0;
            for (Player* p : others)
            {
                float const d = Dist2D(bot->GetPositionX(), bot->GetPositionY(), p->GetPositionX(), p->GetPositionY());
                nearest = std::min(nearest, d);
                if (d <= 18.0f)
                    ++within;
            }
            // Who carries it, with how much health and how many raiders the blast reaches, once per bomb (Geddon 2026-10-13 5b31e3df: a caster
            // died to the Explosion 29 y from Geddon and no line said whose bomb it was).
            if (Aura const* bomb = bot->GetAura(rule.aura))
                if (uint32_t& seen = g_lastBombLog[bot->GetGUID().GetCounter()]; now - seen >= 12000u)
                {
                    seen = now;
                    TC_LOG_INFO("playerbot.v2.raid", "[raid_bomb] {} carries aura {} (class {}, {} health, {} ms left): nearest raider {:.1f} y, {} within 18 y",
                        bot->GetName(), rule.aura, uint32(bot->GetClass()), bot->GetHealth(), bomb->GetDuration(), nearest, within);
                }
            // Already clear: stand still, do not run into other hazards. The stand is an order, not the absence of one: without it the
            // formation and cast-reach rules walked the carrier back toward the raid, it was clear again a second later and ran out again, and
            // the bomb went off 36-41 y from Geddon in the middle of the casters (n14 mc_geddon: Mclockb alternated carrier run, formation
            // and "steps in" for 8 s; a healer died to the 3200 Explosion).
            // A carrier standing in a hazard circle (Inferno) still leaves it: the bomb is 3200 on top of ticks up to 5000, and standing in the
            // circle for the whole bomb timer killed Mctankda at 12 y from Geddon (n17 mc_geddon run 3). The hazard escape below picks the way
            // out; the carrier circles of other bombs are already in `circles`.
            bool const inHazard = !circles.empty() && InsideAny(circles, bot->GetPositionX(), bot->GetPositionY(), 0.0f);
            if (nearest >= rule.clearDistance - 1.0f)
            {
                if (inHazard)
                    break;
                SetMove(out, bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), "carrier hold");
                return;
            }
            g_carrierRun = true;
            bool const found = Clearest(bot, others, rule.clearDistance, out, "carrier run", circles, /*urgent*/ true);
            g_carrierRun = false;
            if (found) { Remember(bot, out, now); return; }
            uint32_t& at = g_lastCarrierLog[bot->GetGUID().GetCounter()];
            if (now - at >= 3000)
            {
                at = now;
                TC_LOG_INFO("playerbot.v2.raid", "[raid] {} carries aura {} but found no reachable clear spot (nearest raider {:.0f} yd; {})",
                    bot->GetName(), rule.aura, nearest, g_clearestWhy);
            }
            if (inHazard)
                break;
            return;
        }

    // 2. Hazards (circles computed by the caller, which also publishes them in the order).
    if (!circles.empty() && InsideAny(circles, bot->GetPositionX(), bot->GetPositionY(), 0.0f))
    {
        // A breath with listed safe points: walk to the nearest usable one (it may lie inside the corridor's padding, so it is not tested
        // against the circles), and stay on it while the corridor is open.
        if (g_breathCast)
        {
            std::vector<BreathSafe const*> spots;
            for (BreathSafe const& s : rules.breathSafe)
                if (s.enemy == g_breathEnemy && s.castSpell == g_breathCast)
                    spots.push_back(&s);
            std::sort(spots.begin(), spots.end(), [&](BreathSafe const* a, BreathSafe const* b)
            {
                return Dist2D(bot->GetPositionX(), bot->GetPositionY(), a->x, a->y) < Dist2D(bot->GetPositionX(), bot->GetPositionY(), b->x, b->y);
            });
            for (BreathSafe const* s : spots)
            {
                float pz = 0.0f;
                if (!Usable(bot, s->x, s->y, bot->GetPositionZ(), pz, /*urgent*/ true, /*hazard*/ true))
                    continue;
                SetMove(out, s->x, s->y, pz, "leave hazard");
                Remember(bot, out, now);
                return;
            }
        }
        if (UseSticky(bot, circles, now, out)) return;
        if (Escape(bot, circles, out, self.tank ? &others : nullptr))
        {
            // How long after the circle opened this member started out, and how far it has to go (MC Geddon 2026-10-13 4839bcf3: six melee died
            // 17.8-21.2 y from the boss, still running).
            if (g_auraCircleOpenedMs && now - g_auraCircleOpenedMs < 8000u && g_auraCircleLeaveLogged[bot->GetGUID().GetCounter()] != g_auraCircleOpenedMs)
            {
                g_auraCircleLeaveLogged[bot->GetGUID().GetCounter()] = g_auraCircleOpenedMs;
                TC_LOG_INFO("playerbot.v2.raid", "[raid_circle] {} starts leaving {} ms after the circle opened, {} circles, going to {:.1f},{:.1f} ({:.1f} y)",
                    bot->GetName(), now - g_auraCircleOpenedMs, circles.size(), out.move_x, out.move_y,
                    Dist2D(bot->GetPositionX(), bot->GetPositionY(), out.move_x, out.move_y));
            }
            Remember(bot, out, now);
            return;
        }
        // Inside a circle with no way out found: the bot stands in it until the next pass. Said once a second per bot, with what refused the
        // probed points (Geddon 2026-10-13 d202b196: six melee died in the Inferno circle and no line said they were stuck).
        uint32_t& stuckAt = g_lastStuckLog[bot->GetGUID().GetCounter()];
        if (now - stuckAt >= 1000u)
        {
            stuckAt = now;
            TC_LOG_INFO("playerbot.v2.raid", "[raid_circle] {} is inside a hazard circle at {:.1f},{:.1f} and found no way out: {} ({} circles, {} urgent probes this pass)",
                bot->GetName(), bot->GetPositionX(), bot->GetPositionY(), g_escapeWhy, circles.size(), g_urgentProbes);
        }
        return;
    }
    // 2a. Inside a keep-out zone (feared, knocked or chased into Onyxia's cave mouth): walk out. Only the end points of planned moves were
    // kept out of it, so a bot that got in some other way stayed there, next to the idle whelps.
    if (!g_keepOut.empty() && InsideAny(g_keepOut, bot->GetPositionX(), bot->GetPositionY(), 0.0f) && Escape(bot, g_keepOut, out, nullptr))
    {
        Remember(bot, out, now);
        return;
    }
    // Melee whose target stands inside a hazard for them: hold instead of chasing back in.
    if (!circles.empty() && !self.tank && !self.healer && !self.ranged)
        if (Unit* victim = bot->GetVictim())
            if (InsideAny(circles, victim->GetPositionX(), victim->GetPositionY(), 0.0f))
                out.hold_melee = true;

    // 2b. Frontal cones: step sideways out of the sector in front of the boss.
    for (AvoidFrontal const& rule : rules.avoidFrontal)
    {
        if (!Matches(rule.who, self, plan.mainTank))
            continue;
        for (Unit* enemy : plan.enemies)
        {
            if (enemy->GetEntry() != rule.enemy || enemy->IsFlying() || !InPhase(enemy, rule.hpMax, rule.hpMin))
                continue;
            float const distance = Dist2D(bot->GetPositionX(), bot->GetPositionY(), enemy->GetPositionX(), enemy->GetPositionY());
            if (distance > rule.radius)
                continue;
            float const half = rule.halfAngleDeg * (float(M_PI) / 180.0f);
            float const facing = enemy->GetOrientation() + rule.facingOffsetDeg * (float(M_PI) / 180.0f);
            float toBot = std::atan2(bot->GetPositionY() - enemy->GetPositionY(), bot->GetPositionX() - enemy->GetPositionX()) - facing;
            while (toBot > float(M_PI)) toBot -= 2.0f * float(M_PI);
            while (toBot < -float(M_PI)) toBot += 2.0f * float(M_PI);
            if (std::fabs(toBot) >= half)
                continue; // already outside the cone
            // Leave through the nearer edge, a few degrees past it, keeping roughly the same distance from the boss (never closer than 6 yards).
            float const side = toBot >= 0.0f ? 1.0f : -1.0f;
            float const ring = std::max(distance, 6.0f);
            int pathChecks = 0;
            for (float extra : { 0.12f, 0.30f, 0.55f })
                for (float flip : { 1.0f, -1.0f })
                {
                    float const angle = facing + side * flip * (half + extra);
                    float const px = enemy->GetPositionX() + ring * std::cos(angle);
                    float const py = enemy->GetPositionY() + ring * std::sin(angle);
                    if (!circles.empty() && InsideAny(circles, px, py, 1.0f))
                        continue;
                    if (++pathChecks > 6)
                        break;
                    float pz = 0.0f;
                    if (!Usable(bot, px, py, bot->GetPositionZ(), pz, /*urgent*/ true, /*hazard*/ true))
                        continue;
                    SetMove(out, px, py, pz, "leave cone");
                    return;
                }
        }
    }

    // 2c. Tank facing: a tank holding an enemy with a frontal cone (breath, cleave, Wing Buffet) puts the enemy's back to the raid.
    // When three or more non-tanks stand inside that cone, the tank steps to the far side of the enemy (on the line from their
    // centre through the enemy, at melee range), so the enemy turns with it and the cone sweeps empty ground instead of making
    // the raid dodge. At most one such step per tank every 8 seconds.
    if (self.tank && !self.healer)
        for (AvoidFrontal const& rule : rules.avoidFrontal)
        {
            if (rule.who != Who::NonTank || !rule.tankTurns || rule.facingOffsetDeg != 0.0f)
                continue;
            for (Unit* enemy : plan.enemies)
            {
                if (enemy->GetEntry() != rule.enemy || !enemy->IsAlive() || enemy->IsFlying() || enemy->GetVictim() != bot ||
                    !InPhase(enemy, rule.hpMax, rule.hpMin))
                    continue;
                float const ex = enemy->GetPositionX(), ey = enemy->GetPositionY();
                float const half = rule.halfAngleDeg * (float(M_PI) / 180.0f);
                float const facing = enemy->GetOrientation();
                float sx = 0.0f, sy = 0.0f;
                int inCone = 0;
                for (Member const& m : plan.members)
                {
                    if (m.tank || m.p == bot)
                        continue;
                    float const mx = m.p->GetPositionX(), my = m.p->GetPositionY();
                    if (Dist2D(mx, my, ex, ey) > rule.radius)
                        continue;
                    float toM = std::atan2(my - ey, mx - ex) - facing;
                    while (toM > float(M_PI)) toM -= 2.0f * float(M_PI);
                    while (toM < -float(M_PI)) toM += 2.0f * float(M_PI);
                    if (std::fabs(toM) < half)
                    {
                        ++inCone;
                        sx += mx; sy += my;
                    }
                }
                if (inCone < 3)
                    continue;
                uint32_t& at = g_lastTankFace[bot->GetGUID().GetCounter()];
                if (now - at < 8000)
                    continue;
                float const away = std::atan2(ey - sy / float(inCone), ex - sx / float(inCone));   // from their centre through the enemy
                float const px = ex + 3.5f * std::cos(away);
                float const py = ey + 3.5f * std::sin(away);
                if (!circles.empty() && InsideAny(circles, px, py, 0.5f))
                    continue;
                float pz = 0.0f;
                if (!Usable(bot, px, py, enemy->GetPositionZ(), pz))
                    continue;
                at = now;
                SetMove(out, px, py, pz, "face boss away");
                return;
            }
        }

    // 3. Spread: keep distance from the nearest raid member while the boss is up.
    for (Spread const& rule : rules.spread)
        if (Matches(rule.who, self, plan.mainTank))
            for (Unit* enemy : plan.enemies)
                if (enemy->GetEntry() == rule.enemy && InPhase(enemy, rule.hpMax, rule.hpMin))
                {
                    float nearest = 9999.0f;
                    for (Player* p : others)
                        nearest = std::min(nearest, Dist2D(bot->GetPositionX(), bot->GetPositionY(), p->GetPositionX(), p->GetPositionY()));
                    if (nearest >= rule.radius * 0.85f)
                        continue;
                    // At most one spread move per bot every 4 seconds, so casters are not hopping around and cancelling their own casts.
                    uint32_t& at = g_lastSpread[bot->GetGUID().GetCounter()];
                    if (now - at < 4000)
                        continue;
                    if (Clearest(bot, others, rule.radius, out, "spread", circles, /*urgent*/ false)) { at = now; Remember(bot, out, now); return; }
                }

    // 3b. Face away (Magmadar): the tank that holds the enemy stands on the far side of it from the raid, so the enemy's back is
    // to everybody else. Re-evaluated every 5 s; a tank already opposite the raid (within 30 degrees) stays put.
    if (self.tank && !self.healer)
        for (FaceAway const& rule : rules.faceAway)
            for (Unit* enemy : plan.enemies)
            {
                if (enemy->GetEntry() != rule.enemy || !enemy->IsAlive() || enemy->GetVictim() != bot ||
                    !InPhase(enemy, rule.hpMax, rule.hpMin))
                    continue;
                float const ex = enemy->GetPositionX(), ey = enemy->GetPositionY();
                float axis;
                if (!FaceAxisFor(plan, enemy, now, axis, rule.bearingDeg))
                    continue;
                if (geo::TankOnAxis(ex, ey, bot->GetPositionX(), bot->GetPositionY(), axis, 30.0f * (float(M_PI) / 180.0f)))
                    continue;
                uint32_t& at = g_lastFaceAway[bot->GetGUID().GetCounter()];
                if (now - at < 5000)
                    continue;
                // The far side of the boss is usually the wall, where a spot 3.5 yards out may have no floor: try shorter reaches and a
                // few degrees either side of the straight line (all within the 30 degrees the check above accepts). One bot, every
                // 5 s, so the probes are urgent: the shared routine budget is spent by the raid's spread and role steps (a 40-man
                // raid), and the tank then never found a spot.
                bool found = false;
                float px = 0.0f, py = 0.0f, pz = 0.0f;
                for (float reach : { 3.5f, 2.8f, 2.2f })
                {
                    for (float turnDeg : { 0.0f, 15.0f, -15.0f, 28.0f, -28.0f })
                    {
                        geo::AxisSpot(ex, ey, axis, reach, turnDeg * (float(M_PI) / 180.0f), px, py);
                        if (!circles.empty() && InsideAny(circles, px, py, 0.5f))
                            continue;
                        if (Usable(bot, px, py, enemy->GetPositionZ(), pz, /*urgent*/ true))
                        {
                            found = true;
                            break;
                        }
                    }
                    if (found)
                        break;
                }
                at = now;
                if (!found)
                {
                    TC_LOG_INFO("playerbot.v2.raid", "[raid_face] {} found no spot on the axis behind {}", bot->GetName(), enemy->GetName());
                    continue;
                }
                TC_LOG_INFO("playerbot.v2.raid", "[raid_face] {} steps behind {} to turn it away from the raid", bot->GetName(), enemy->GetName());
                SetMove(out, px, py, pz, "face boss away");
                return;
            }

    // 3c. Stand behind (Magmadar): matching bots stand in the sector behind the enemy, wherever the tank has turned it.
    // At most one step per bot every 4 s, so a boss that turns a little does not make the raid hop.
    for (StandBehind const& rule : rules.standBehind)
    {
        if (!Matches(rule.who, self, plan.mainTank) || self.tank)
            continue;
        for (Unit* enemy : plan.enemies)
        {
            if (enemy->GetEntry() != rule.enemy || !enemy->IsAlive() || !InPhase(enemy, rule.hpMax, rule.hpMin))
                continue;
            float const ex = enemy->GetPositionX(), ey = enemy->GetPositionY();
            // The boss faces its tank, i.e. along the locked axis; its live facing swings with every tank step and every swing of its
            // aim, which is what sent the DPS round the boss over and over.
            float eo = enemy->GetOrientation();
            if (!rules.faceAway.empty())
                for (FaceAway const& fa : rules.faceAway)
                    if (fa.enemy == rule.enemy)
                    {
                        float axis;
                        if (FaceAxisFor(plan, enemy, now, axis, fa.bearingDeg))
                            eo = axis;
                        break;
                    }
            float const half = rule.halfAngleDeg * (float(M_PI) / 180.0f);
            if (geo::InRearSector(ex, ey, eo, bot->GetPositionX(), bot->GetPositionY(), half, rule.minDist, rule.maxDist))
                break;
            uint32_t& at = g_lastRearStep[bot->GetGUID().GetCounter()];
            if (now - at < 4000)
                break;
            float px, py;
            geo::RearSpot(ex, ey, eo, bot->GetPositionX(), bot->GetPositionY(), half, rule.minDist, rule.maxDist, px, py);
            if (!circles.empty() && InsideAny(circles, px, py, 1.0f))
                break;
            float pz = 0.0f;
            if (!Usable(bot, px, py, enemy->GetPositionZ(), pz))
                break;
            at = now;
            SetMove(out, px, py, pz, "behind boss");
            return;
        }
    }

    // 3d. Stand on the flank (Onyxia): matching bots stand in the wedge beside the enemy, between its front cones and its tail sweep, on the
    // side they are on. A bot already in the wedge stays; at most one step per bot every 4 s.
    for (StandFlank const& rule : rules.standFlank)
    {
        if (!Matches(rule.who, self, plan.mainTank) || self.tank)
            continue;
        for (Unit* enemy : plan.enemies)
        {
            if (enemy->GetEntry() != rule.enemy || !enemy->IsAlive() || enemy->IsFlying() || !InPhase(enemy, rule.hpMax, rule.hpMin))
                continue;
            float const ex = enemy->GetPositionX(), ey = enemy->GetPositionY();
            float const eo = enemy->GetOrientation();
            float const half = rule.halfAngleDeg * (float(M_PI) / 180.0f), centre = rule.centerDeg * (float(M_PI) / 180.0f);
            // Which flank: the side of its facing the bot is on. The rear-sector helpers measure from the line straight behind a facing, so the
            // flank is handed to them as the "rear" of a facing turned a quarter round the other way.
            float const side = geo::NormalizeAngle(geo::Bearing(ex, ey, bot->GetPositionX(), bot->GetPositionY()) - eo) >= 0.0f ? 1.0f : -1.0f;
            // One slot per bot: the matching members on the same side, in guid order (plan.members is sorted), share the wedge as a grid
            // of rows (distance) and columns (angle). A bot only walks to its own slot, so nothing else moves it and nobody lands on the
            // same edge point (phase 3 live 2026-10-15: the raid in one clump beside her).
            uint32_t index = 0, count = 0;
            for (Member const& other : plan.members)
            {
                if (!Matches(rule.who, other, plan.mainTank) || other.tank)
                    continue;
                float const otherSide = geo::NormalizeAngle(geo::Bearing(ex, ey, other.p->GetPositionX(), other.p->GetPositionY()) - eo) >= 0.0f ? 1.0f : -1.0f;
                if (otherSide != side)
                    continue;
                if (other.p == self.p)
                    index = count;
                ++count;
            }
            float const depth = std::max(rule.maxDist - rule.minDist, 1.0f);
            uint32_t const rows = std::clamp(uint32_t(std::ceil(depth / 5.0f)), 1u, 4u);
            uint32_t const cols = std::max(1u, (count + rows - 1) / rows);
            uint32_t const row = index % rows, col = index / rows;
            float const angle = centre - half + (float(col) + 0.5f) / float(cols) * 2.0f * half;
            float const dist = rule.minDist + (float(row) + 0.5f) / float(rows) * depth;
            float const px = ex + dist * std::cos(eo + side * angle), py = ey + dist * std::sin(eo + side * angle);
            if (Dist2D(bot->GetPositionX(), bot->GetPositionY(), px, py) <= 3.0f)
                break;
            uint32_t& at = g_lastRearStep[bot->GetGUID().GetCounter()];
            if (now - at < 4000)
                break;
            if (!circles.empty() && InsideAny(circles, px, py, 1.0f))
                break;
            float pz = 0.0f;
            if (!Usable(bot, px, py, enemy->GetPositionZ(), pz))
                break;
            at = now;
            SetMove(out, px, py, pz, "flank boss");
            return;
        }
    }

    // 4. Role positions.
    for (StandByAura const& rule : rules.standByAura)
    {
        if (!bot->HasAura(rule.aura))
            continue;
        for (Unit* enemy : plan.enemies)
        {
            if (enemy->GetEntry() != rule.enemy)
                continue;
            float const sx = enemy->GetPositionX() + rule.distance * std::cos(rule.bearing);
            float const sy = enemy->GetPositionY() + rule.distance * std::sin(rule.bearing);
            if (Dist2D(bot->GetPositionX(), bot->GetPositionY(), sx, sy) <= rule.tolerance)
                break;
            float sz = 0.0f;
            if (Usable(bot, sx, sy, enemy->GetPositionZ(), sz))
            {
                SetMove(out, sx, sy, sz, "charge side");
                return;
            }
            break;
        }
    }
    for (Stand const& rule : rules.stand)
    {
        if (!Matches(rule.who, self, plan.mainTank))
            continue;
        if (rule.share)
        {
            // Position among the matching members (plan.members is sorted by guid): odd and even ones go to different posts.
            uint32_t index = 0;
            for (Member const& other : plan.members)
            {
                if (other.p == self.p)
                    break;
                if (Matches(rule.who, other, plan.mainTank))
                    ++index;
            }
            if ((index & 1u) != (rule.share == 2 ? 1u : 0u))
                continue;
        }
        if (rule.who == Who::Healer)
        {
            // Healer camp positions only make sense when there are two tanks to be healed at two camps.
            int livingTanks = 0;
            for (Member const& m : plan.members)
                if (m.tank) ++livingTanks;
            if (livingTanks < 2)
                continue;
        }
        Unit* boss = nullptr;
        Unit* other = nullptr;
        for (Unit* enemy : plan.enemies)
        {
            if (enemy->GetEntry() == rule.enemy && InPhase(enemy, rule.hpMax, rule.hpMin)) boss = enemy;
            if (rule.needOtherEnemy && enemy->GetEntry() == rule.needOtherEnemy && (!rule.needVictimIsMe || enemy->GetVictim() == bot))
                other = enemy;
        }
        if (!boss)
            continue;
        if (rule.needOtherEnemy ? !other : (rule.needVictimIsMe && boss->GetVictim() != bot))
            continue;
        if (Dist2D(bot->GetPositionX(), bot->GetPositionY(), rule.x, rule.y) <= rule.tolerance)
            continue;
        SetMove(out, rule.x, rule.y, rule.z, "role position");
        return;
    }

    // 5. Stay near: back inside the radius of the nearest matching enemy.
    {
        float nearestRadius = 0.0f;
        Unit* nearest = StayNearEnemy(plan, self, nearestRadius);
        float const nearestDist = nearest
            ? Dist2D(bot->GetPositionX(), bot->GetPositionY(), nearest->GetPositionX(), nearest->GetPositionY()) : 0.0f;
        if (nearest && nearestDist > nearestRadius)
        {
            float const ex = nearest->GetPositionX(), ey = nearest->GetPositionY();
            float const base = std::atan2(bot->GetPositionY() - ey, bot->GetPositionX() - ex);
            float const ring = nearestRadius * 0.6f;
            for (float turn : { 0.0f, 0.5f, -0.5f, 1.0f, -1.0f })
            {
                float const px = ex + ring * std::cos(base + turn);
                float const py = ey + ring * std::sin(base + turn);
                if (!circles.empty() && InsideAny(circles, px, py, 1.0f))
                    continue;
                float pz = 0.0f;
                if (!Usable(bot, px, py, nearest->GetPositionZ(), pz))
                    continue;
                SetMove(out, px, py, pz, "stay near");
                return;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Elections (port of ike3 DecideRaidCast)
// ---------------------------------------------------------------------------
uint32_t Stacks(Unit* u, uint32_t aura)
{
    Aura* a = u->GetAura(aura);
    return a ? std::max<uint32_t>(a->GetStackAmount(), 1u) : 0u;
}

Member* FindMember(Plan& plan, Player* p)
{
    for (Member& m : plan.members)
        if (m.p == p)
            return &m;
    return nullptr;
}

RaidOrder* Election::OrderOf(Player* p)
{
    auto it = orders.find(p->GetGUID().GetCounter());
    return it == orders.end() ? nullptr : &it->second;
}

bool Election::Assign(Unit* target, std::vector<Ranks const*> const& families, bool hostile, RaidOrder::CastKind kind, char const* what,
                      bool skipHealersInEmergency, int minHealerManaPct)
{
    for (bool allowBusy : { false, true })
        for (Member& m : plan.members)
        {
            if (!m.bot || busy.count(m.p->GetGUID().GetCounter()))
                continue;
            if (skipHealersInEmergency && plan.emergency && m.healer)
                continue;
            if (minHealerManaPct > 0 && m.healer && m.p->GetMaxPower(POWER_MANA) > 0
                && int(uint64_t(m.p->GetPower(POWER_MANA)) * 100 / uint64_t(m.p->GetMaxPower(POWER_MANA))) < minHealerManaPct)
                continue;
            if (Busy(m.p) != allowBusy)
                continue;
            for (Ranks const* family : families)
            {
                uint32_t const id = KnownRank(m.p, *family);
                if (!Ready(m.p, id, target, hostile))
                    continue;
                RaidOrder* o = OrderOf(m.p);
                if (!o)
                    return false;
                o->cast_kind = kind;
                o->cast_spell = id;
                o->cast_target = target->GetGUID();
                busy.insert(m.p->GetGUID().GetCounter());
                dump << "  " << m.p->GetName() << ": " << what << " " << id << " -> " << target->GetName() << "\n";
                return true;
            }
        }
    return false;
}

void ElectTankSwaps(Election& e)
{
    Plan& plan = e.plan;
    for (TankSwap const& rule : plan.rules->tankSwap)
        for (Unit* enemy : plan.enemies)
        {
            if (enemy->GetEntry() != rule.enemy || Controlled(enemy))
                continue;
            Unit* held = enemy->GetVictim();
            Player* holder = held ? held->ToPlayer() : nullptr;
            Member* hm = holder ? FindMember(plan, holder) : nullptr;
            uint32_t const need = std::max<uint32_t>(rule.stacks, 1u);
            // The PvE coordinator's raid tank assignment follows whoever holds the enemy, so its "get onto your mob" rule
            // never drags the debuffed tank back or pulls the new one away after a swap.
            if (hm && hm->tank && hm->bot)
            {
                PveOrder const* po = Services::PveCoordinator().OrderFor(holder->GetGUID().GetCounter());
                if (po && po->active && po->tank_target != enemy->GetGUID())
                {
                    Services::PveCoordinator().ReassignTankTarget(plan.groupLow, plan.mapId, holder->GetGUID().GetCounter(), enemy->GetGUID());
                    e.dump << "  " << holder->GetName() << ": now owns " << enemy->GetName() << " in the tank assignment\n";
                }
            }
            // A tank that carries the debuff and has been taunted off stops attacking this enemy, so it does not take it back.
            for (Member const& m : plan.members)
                if (m.tank && m.bot && m.p != holder && Stacks(m.p, rule.debuffAura) >= need)
                    if (RaidOrder* o = e.OrderOf(m.p))
                        o->forbidden_targets.push_back(enemy->GetGUID());
            if (!hm || !hm->tank || Stacks(holder, rule.debuffAura) < need)
                continue;
            bool swapped = false;
            // The clean tank nearest to the enemy takes it: one that stands in melee reach keeps the boss in front of the raid, one that
            // was knocked back would leave him with nobody to hit (Ragnaros 2026-10-15: no tank within 20 y of him for seconds after Wrath).
            std::vector<Member*> clean;
            for (Member& m : plan.members)
                clean.push_back(&m);
            std::stable_sort(clean.begin(), clean.end(), [&](Member* a, Member* b) { return a->p->GetDistance(enemy) < b->p->GetDistance(enemy); });
            for (Member* mp : clean)
            {
                Member& m = *mp;
                if (m.p == holder || !m.tank || !m.bot || Stacks(m.p, rule.debuffAura) > 0 || e.busy.count(m.p->GetGUID().GetCounter()))
                    continue;
                bool taken = false;
                for (Ranks const* family : { &kTaunt, &kGrowl })
                {
                    uint32_t const id = KnownRank(m.p, *family);
                    if (!Ready(m.p, id, enemy, true))
                        continue;
                    if (RaidOrder* o = e.OrderOf(m.p))
                    {
                        o->cast_kind = RaidOrder::CastKind::Taunt;
                        o->cast_spell = id;
                        o->cast_target = enemy->GetGUID();
                        e.busy.insert(m.p->GetGUID().GetCounter());
                        // Hand the enemy over in the tank assignment now, so the new tank stays on it after the taunt.
                        Services::PveCoordinator().ReassignTankTarget(plan.groupLow, plan.mapId, m.p->GetGUID().GetCounter(), enemy->GetGUID());
                        e.dump << "  " << m.p->GetName() << ": taunt swap " << enemy->GetName() << " off " << holder->GetName() << "\n";
                        TC_LOG_INFO("playerbot.v2", "[raid_swap] {} taunts {} off {} ({} stacks of {}), {:.0f}y from it", m.p->GetName(), enemy->GetName(),
                                    holder->GetName(), Stacks(holder, rule.debuffAura), rule.debuffAura, m.p->GetDistance(enemy));
                        taken = true;
                    }
                    break;
                }
                if (taken)
                {
                    swapped = true;
                    break;
                }
            }
            if (swapped)
                continue;
            // Nobody clean is in taunt range (Four Horsemen: the next tank waits in another corner). The nearest clean tank that
            // knows a taunt walks over; it taunts once it is in range on a later plan.
            Member* nearest = nullptr;
            float nearestDist = 0.0f;
            for (Member& m : plan.members)
            {
                if (m.p == holder || !m.tank || !m.bot || Stacks(m.p, rule.debuffAura) > 0 || e.busy.count(m.p->GetGUID().GetCounter()))
                    continue;
                if (!KnownRank(m.p, kTaunt) && !KnownRank(m.p, kGrowl))
                    continue;
                float const d = m.p->GetDistance(enemy);
                if (!nearest || d < nearestDist) { nearest = &m; nearestDist = d; }
            }
            if (!nearest || nearestDist < 10.0f)
                continue;
            RaidOrder* o = e.OrderOf(nearest->p);
            if (!o || o->move)
                continue;
            float const angle = std::atan2(nearest->p->GetPositionY() - enemy->GetPositionY(), nearest->p->GetPositionX() - enemy->GetPositionX());
            float const px = enemy->GetPositionX() + 8.0f * std::cos(angle);
            float const py = enemy->GetPositionY() + 8.0f * std::sin(angle);
            float pz = 0.0f;
            if (!Usable(nearest->p, px, py, enemy->GetPositionZ(), pz))
                continue;
            SetMove(*o, px, py, pz, "tank swap: take over");
            e.busy.insert(nearest->p->GetGUID().GetCounter());
            e.dump << "  " << nearest->p->GetName() << ": walks to " << enemy->GetName() << " to take it off " << holder->GetName() << "\n";
        }
}

// A boss that a Blink, a fear or a death left on a non-tank (Shazzrah's Gate of Shazzrah resets his threat and teleports him to a
// random player; Magmadar's Panic scatters the tanks; Ragnaros' Wrath resets it): a tank taunts it back at once, the main tank first, and
// takes the tank assignment with it. The bosses are RaidRules::retaunt. Logged as [raid_retaunt].
void ElectRetaunt(Election& e)
{
    Plan& plan = e.plan;
    for (Unit* enemy : plan.enemies)
    {
        if (std::find(plan.rules->retaunt.begin(), plan.rules->retaunt.end(), enemy->GetEntry()) == plan.rules->retaunt.end() || Controlled(enemy))
            continue;
        Unit* held = enemy->GetVictim();
        Player* holder = held ? held->ToPlayer() : nullptr;
        Member* hm = holder ? FindMember(plan, holder) : nullptr;
        if (!hm || hm->tank)
            continue;
        for (int pass = 0; pass < 2; ++pass)
            for (Member& m : plan.members)
            {
                if (!m.tank || !m.bot || m.p == holder || e.busy.count(m.p->GetGUID().GetCounter()))
                    continue;
                if ((pass == 0) != (m.p == plan.mainTank))
                    continue;
                for (Ranks const* family : { &kTaunt, &kGrowl })
                {
                    uint32_t const id = KnownRank(m.p, *family);
                    if (!Ready(m.p, id, enemy, true))
                        continue;
                    RaidOrder* o = e.OrderOf(m.p);
                    if (!o)
                        break;
                    o->cast_kind = RaidOrder::CastKind::Taunt;
                    o->cast_spell = id;
                    o->cast_target = enemy->GetGUID();
                    e.busy.insert(m.p->GetGUID().GetCounter());
                    Services::PveCoordinator().ReassignTankTarget(plan.groupLow, plan.mapId, m.p->GetGUID().GetCounter(), enemy->GetGUID());
                    e.dump << "  " << m.p->GetName() << ": re-taunts " << enemy->GetName() << " off " << holder->GetName() << "\n";
                    static std::unordered_map<uint64_t, uint32_t> lastLog;
                    uint32_t const now = GameTime::GetGameTimeMS();
                    uint32_t& l = lastLog[m.p->GetGUID().GetCounter()];
                    if (!l || now - l >= 5000u)
                    {
                        l = now ? now : 1u;
                        TC_LOG_INFO("playerbot.v2.raid", "[raid_retaunt] {} taunts {} back: it was on {} (not a tank)", m.p->GetName(),
                                    enemy->GetName(), holder->GetName());
                    }
                    goto next_enemy;
                }
            }
    next_enemy:;
    }
}

// Magmadar's Panic (19408, about every 30 s): the main warrior tank steps into Berserker Stance a moment before it is due, casts
// Berserker Rage (fear immunity) and goes straight back to Defensive Stance. Panic's timing is learned from the first Panic
// seen on any raid member; before that the tank stays put. The off tanks stay in Defensive and rely on the re-taunt.
void ElectBerserkerDance(Election& e)
{
    constexpr uint32_t kMagmadar = 11982, kPanic = 19408, kBerserkerStance = 2458, kDefensiveStance = 71, kBerserkerRage = 18499;
    constexpr uint32_t kPanicCycleMs = 30000, kLeadMs = 3000, kStepTimeoutMs = 4000;
    struct State { uint32_t lastPanic = 0, phaseStart = 0; int phase = 0; };
    static std::unordered_map<uint64_t, State> states;
    Plan& plan = e.plan;
    bool magmadar = false;
    for (Unit* enemy : plan.enemies)
        magmadar = magmadar || enemy->GetEntry() == kMagmadar;
    if (!magmadar)
    {
        states.erase(plan.groupLow);
        return;
    }
    State& st = states[plan.groupLow];
    uint32_t const now = GameTime::GetGameTimeMS();
    for (Member const& m : plan.members)
        if (m.p->HasAura(kPanic) && (!st.lastPanic || now - st.lastPanic > 5000u))
            st.lastPanic = now ? now : 1u;
    Member* tank = nullptr;
    for (Member& m : plan.members)
        if (m.bot && m.tank && m.p == plan.mainTank && m.p->GetClass() == CLASS_WARRIOR)
            tank = &m;
    if (!tank || e.busy.count(tank->p->GetGUID().GetCounter()))
    {
        st.phase = 0;
        return;
    }
    Player* t = tank->p;
    auto order = [&](uint32_t spell, char const* what)
    {
        if (!Ready(t, spell, t, false))
            return false;
        RaidOrder* o = e.OrderOf(t);
        if (!o)
            return false;
        o->cast_kind = RaidOrder::CastKind::StanceDance;
        o->cast_spell = spell;
        o->cast_target = t->GetGUID();
        e.busy.insert(t->GetGUID().GetCounter());
        e.dump << "  " << t->GetName() << ": Magmadar Panic dance, " << what << "\n";
        return true;
    };
    switch (st.phase)
    {
        case 0:
            if (st.lastPanic && now - st.lastPanic + kLeadMs >= kPanicCycleMs && now - st.lastPanic < kPanicCycleMs + 6000u)
            {
                st.phase = 1;
                st.phaseStart = now;
                TC_LOG_INFO("playerbot.v2.raid", "[raid_panic] {} steps into Berserker Stance for Magmadar's Panic", t->GetName());
            }
            break;
        case 1:
            if (t->GetShapeshiftForm() == FORM_BERSERKER_STANCE)
                st.phase = 2;
            else if (now - st.phaseStart > kStepTimeoutMs)
                st.phase = 3;
            else
                order(kBerserkerStance, "to Berserker Stance");
            if (st.phase != 2)
                break;
            [[fallthrough]];
        case 2:
            if (t->HasAura(kBerserkerRage) || now - st.phaseStart > 2 * kStepTimeoutMs)
                st.phase = 3;
            else
                order(kBerserkerRage, "Berserker Rage");
            break;
        case 3:
            if (t->GetShapeshiftForm() == FORM_DEFENSIVE_STANCE || now - st.phaseStart > 3 * kStepTimeoutMs)
            {
                st.phase = 0;
                if (!st.lastPanic || now - st.lastPanic > kPanicCycleMs + 6000u)
                    st.lastPanic = now ? now : 1u;   // Panic not seen this cycle: try again one cycle on
            }
            else
                order(kDefensiveStance, "back to Defensive Stance");
            break;
        default:
            st.phase = 0;
            break;
    }
}

// Melee soak (Patchwerk's Hateful Strike): the matching bots keep attacking the enemy in melee so they stay among its top
// threat and soak the strike instead of a DPS. Never the main tank, at most rule.count bots, lowest guid first.
void PublishMeleeSoak(Plan& plan, std::unordered_map<uint64_t, RaidOrder>& orders)
{
    for (MeleeSoak const& rule : plan.rules->meleeSoak)
        for (Unit* enemy : plan.enemies)
        {
            if (enemy->GetEntry() != rule.enemy)
                continue;
            uint32_t taken = 0;
            for (Member const& m : plan.members)
            {
                if (taken >= rule.count)
                    break;
                if (!m.bot || m.p == plan.mainTank || !Matches(rule.who, m, plan.mainTank))
                    continue;
                auto it = orders.find(m.p->GetGUID().GetCounter());
                if (it == orders.end())
                    continue;
                it->second.attack_target = enemy->GetGUID();
                ++taken;
            }
        }
}

bool MechanicsMove(Plan const& plan, Member const& self, uint32_t now, RaidOrder& out)
{
    switch (plan.mapId)
    {
        case 531: return Aq40MechanicsMove(plan, self, now, out);
        case 533: return NaxxMechanicsMove(plan, self, now, out);
        default:  return false;
    }
}

void MechanicsPrePull(Plan& plan, std::unordered_map<uint64_t, RaidOrder>& orders, std::ostringstream& dump, uint32_t now)
{
    if (plan.mapId == 533)
        NaxxPrePull(plan, orders, dump);
    else if (plan.mapId == 469)
        BwlPrePull(plan, orders, dump, now);
}

void MechanicsElect(Election& e, uint32_t now)
{
    switch (e.plan.mapId)
    {
        case 469: BwlMechanicsElect(e, now); break;
        case 531: Aq40MechanicsElect(e, now); break;
        case 533: NaxxMechanicsElect(e, now); break;
        default:  break;
    }
}

// HoldByAura and CancelAura: per-bot reactions to an aura on the bot itself (Nefarian's class calls).
void PublishAuraReactions(Plan& plan, std::unordered_map<uint64_t, RaidOrder>& orders)
{
    if (plan.rules->holdByAura.empty() && plan.rules->cancelAura.empty())
        return;
    for (Member const& m : plan.members)
    {
        if (!m.bot)
            continue;
        auto it = orders.find(m.p->GetGUID().GetCounter());
        if (it == orders.end())
            continue;
        RaidOrder& o = it->second;
        for (HoldByAura const& rule : plan.rules->holdByAura)
            if (Matches(rule.who, m, plan.mainTank) && m.p->HasAura(rule.aura))
                o.hold_rotation = true;
        for (CancelAura const& rule : plan.rules->cancelAura)
            if (!o.cancel_aura && Matches(rule.who, m, plan.mainTank) && m.p->HasAura(rule.aura)
                && std::any_of(plan.enemies.begin(), plan.enemies.end(), [&](Unit* u) { return u->GetEntry() == rule.enemy; }))
                o.cancel_aura = rule.aura;
    }
}

void ElectDispels(Election& e)
{
    Plan& plan = e.plan;
    for (Dispel const& rule : plan.rules->dispel)
    {
        // The data says nobody can dispel it (a fear with no dispel type): no order, or the dispellers spend their globals on casts the
        // core refuses.
        if (SpellInfo const* info = sSpellMgr->GetSpellInfo(rule.aura, DIFFICULTY_NONE))
            if (info->Dispel == DISPEL_NONE)
                continue;
        auto const families = DispelSpellsFor(KindOf(rule));
        // Healers and tanks first for an urgent debuff: Soul Burn silences, and a silenced healer is a dead tank.
        std::vector<Member*> carriers;
        for (Member& m : plan.members)
            if (m.p->HasAura(rule.aura) && DispelWorthIt(rule.urgent, rule.hurtBelowPct, int(m.p->GetHealthPct())))
                carriers.push_back(&m);
        if (rule.urgent)
            std::stable_sort(carriers.begin(), carriers.end(),
                [](Member const* a, Member const* b) { return (a->healer || a->tank) > (b->healer || b->tank); });
        uint32_t given = 0;
        for (Member* m : carriers)
        {
            if (!rule.urgent && given >= kNonUrgentDispelOrdersPerPlan)
                break;
            bool const ok = e.Assign(m->p, families, false, RaidOrder::CastKind::Dispel, "dispel", !rule.urgent,
                                     rule.urgent ? 0 : kNonUrgentDispelHealerManaPct);
            given += ok ? 1u : 0u;
            // An urgent curse on a tank or healer nobody could reach: the nearest idle cleaner (a mage, or a non-healing druid)
            // walks into spell range of it, so the next wave gets cast instead of "no ready cleaner in range and sight".
            if (ok || !rule.urgent || !(m->tank || m->healer))
                continue;
            Member* best = nullptr;
            float bestD = 1e9f;
            for (Member& c : plan.members)
            {
                if (!c.bot || c.healer || c.tank || c.p == m->p || e.busy.count(c.p->GetGUID().GetCounter()))
                    continue;
                if (c.p->GetClass() != CLASS_MAGE && c.p->GetClass() != CLASS_DRUID)
                    continue;
                if (c.p->IsNonMeleeSpellCast(false) || (!KnownRank(c.p, *families.front()) && !KnownRank(c.p, *families.back())))
                    continue;
                float const d = c.p->GetDistance(m->p);
                if (d > 30.0f && d < bestD) { bestD = d; best = &c; }
            }
            if (!best)
                continue;
            RaidOrder* o = e.OrderOf(best->p);
            if (!o || o->move)
                continue;
            // 22 y from the patient on the line to the cleaner: inside the 30 y cure range with room to spare.
            float const dx = best->p->GetPositionX() - m->p->GetPositionX(), dy = best->p->GetPositionY() - m->p->GetPositionY();
            float const len = std::max(std::sqrt(dx * dx + dy * dy), 0.1f);
            float const px = m->p->GetPositionX() + dx / len * 22.0f, py = m->p->GetPositionY() + dy / len * 22.0f;
            float pz = 0.0f;
            if (Usable(best->p, px, py, best->p->GetPositionZ(), pz, true))
            {
                SetMove(*o, px, py, pz, "cleaner reach");
                e.busy.insert(best->p->GetGUID().GetCounter());
                e.dump << "  " << best->p->GetName() << ": move (cleaner reach) to cure " << m->p->GetName() << "\n";
            }
        }
        // Paced per aura: the test-night digest shows whether a debuff nobody removed was seen at all.
        static std::unordered_map<uint32_t, uint32_t> s_lastLogMs;
        uint32_t const nowMs = GameTime::GetGameTimeMS();
        if (!carriers.empty() && nowMs - s_lastLogMs[rule.aura] >= 4000u)
        {
            s_lastLogMs[rule.aura] = nowMs;
            TC_LOG_INFO("playerbot.v2.raid", "[raid_dispel] aura {} on {} members, {} orders given{}", rule.aura, carriers.size(), given,
                given < carriers.size() ? " (the rest: no ready cleaner in range and line of sight)" : "");
        }
    }
}

void ElectPurges(Election& e)
{
    Plan& plan = e.plan;
    for (Unit* enemy : plan.enemies)
        for (PurgeEnemy const& rule : plan.rules->purge)
            if (enemy->HasAura(rule.aura))
                e.Assign(enemy, { &kPurge, &kDispelMagic }, true, RaidOrder::CastKind::Purge, "purge", true);
}

void ElectCrowdControl(Election& e, std::vector<Unit*> const& tankTargets, ObjectGuid killFocus)
{
    Plan& plan = e.plan;
    if (plan.rules->crowdControl.empty())
        return;
    // A caster already holding one add keeps it; it does not take a second.
    auto controlling = [&](Player* p)
    {
        for (Unit* enemy : plan.enemies)
            if (Controlled(enemy))
                for (Ranks const* family : { &kBanish, &kShackle, &kHibernate, &kPolymorph })
                    for (uint32_t id : *family)
                        if (enemy->GetAura(id, p->GetGUID()))
                            return true;
        return false;
    };
    // oneAtATime rules (Garr's Firesworn): the holders keep every add banished while any is loose; once none of the entry is left
    // uncontrolled, one is let go for good - nobody re-banishes it, a tank takes it, the raid kills it - and the next goes only after it
    // is dead. Released guids are remembered until they leave the plan (dead or out of the fight).
    static std::unordered_set<uint64_t> sReleased;
    for (auto it = sReleased.begin(); it != sReleased.end();)
        it = std::any_of(plan.enemies.begin(), plan.enemies.end(), [&](Unit* u) { return u->GetGUID().GetCounter() == *it && u->IsAlive(); })
            ? std::next(it) : sReleased.erase(it);
    for (CrowdControl const& rule : plan.rules->crowdControl)
    {
        if (!rule.oneAtATime)
            continue;
        std::vector<Unit*> held;
        bool loose = false;
        for (Unit* u : plan.enemies)
        {
            if (u->GetEntry() != rule.enemy || !u->IsAlive())
                continue;
            if (sReleased.count(u->GetGUID().GetCounter()))
                loose = true;
            else if (Controlled(u))
                held.push_back(u);
            else
                loose = true;
        }
        if (loose || held.empty())
            continue;
        // The most hurt one first (it dies soonest), lowest guid as the tie-break.
        Unit* pick = *std::min_element(held.begin(), held.end(), [](Unit* a, Unit* b)
        {
            return a->GetHealthPct() != b->GetHealthPct() ? a->GetHealthPct() < b->GetHealthPct() : a->GetGUID().GetCounter() < b->GetGUID().GetCounter();
        });
        sReleased.insert(pick->GetGUID().GetCounter());
        TC_LOG_INFO("playerbot.v2.raid", "[raid_cc_release] entry {}: every unheld one is dead, letting {} ({}) go at {}% ({} still held)",
            rule.enemy, pick->GetName(), pick->GetGUID().GetCounter(), uint32_t(pick->GetHealthPct()), uint32_t(held.size() - 1));
    }
    for (Unit* enemy : plan.enemies)
    {
        if (Controlled(enemy) || std::find(tankTargets.begin(), tankTargets.end(), enemy) != tankTargets.end())
            continue;
        if (sReleased.count(enemy->GetGUID().GetCounter()))
            continue;   // let go on purpose: it is the raid's to kill now
        if (!killFocus.IsEmpty() && enemy->GetGUID() == killFocus)
            continue;   // the mob the raid is burning is never the one to hold
        for (CrowdControl const& rule : plan.rules->crowdControl)
        {
            if (rule.enemy != enemy->GetEntry() || (rule.skipIfAura && enemy->HasAura(rule.skipIfAura)))
                continue;
            if (rule.maxActive
                && uint32_t(std::count_if(plan.enemies.begin(), plan.enemies.end(),
                       [&](Unit* u) { return u->GetEntry() == rule.enemy && Controlled(u); })) >= rule.maxActive)
                break;
            for (Member& m : plan.members)
                if (m.bot && controlling(m.p))
                    e.busy.insert(m.p->GetGUID().GetCounter());
            e.Assign(enemy, { &CcSpellsFor(rule.kind) }, true, RaidOrder::CastKind::CrowdControl, "crowd control", true);
            break;
        }
    }
}

// Mind-controlled raid members (ControlledMember): nobody attacks them; a mage polymorphs one, else somebody stuns it.
// Recomputed every plan from the aura, so everything stops as soon as the control ends.
void ElectControlledMembers(Election& e)
{
    Plan& plan = e.plan;
    if (plan.rules->controlledMember.empty())
        return;
    for (ControlledMember const& rule : plan.rules->controlledMember)
        for (Member const& victim : plan.members)
        {
            Player* p = victim.p;
            if (!p->HasAura(rule.aura))
                continue;
            for (Member const& m : plan.members)
                if (m.bot && m.p != p)
                    if (RaidOrder* o = e.OrderOf(m.p))
                        o->forbidden_targets.push_back(p->GetGUID());
            if (Controlled(p) || p->HasUnitState(UNIT_STATE_STUNNED))
                continue;   // already out of the fight
            if (!e.Assign(p, { &kPolymorph }, true, RaidOrder::CastKind::CrowdControl, "polymorph controlled", true))
                e.Assign(p, { &kHammerOfJustice, &kGouge, &kBash }, true, RaidOrder::CastKind::CrowdControl, "stun controlled", true);
        }
}

// Forbidden targets and the fallback a DPS bot attacks instead.
void PublishTargets(Plan& plan, std::unordered_map<uint64_t, RaidOrder>& orders, ObjectGuid coordFocus)
{
    RaidRules const& rules = *plan.rules;
    std::vector<ObjectGuid> forbiddenAll;      // nobody attacks these
    std::vector<ObjectGuid> forbiddenDps;      // non-tanks do not attack these
    // Packs that die together: downed members are left alone by everybody, members trailing the healthiest one by more than
    // the rule's spread are left alone by DPS, and only the healthiest one can become the DPS fallback or the group focus.
    std::vector<Unit*> packHolders;            // per pack rule with a member up: the healthiest member
    for (KillTogether const& rule : rules.killTogether)
    {
        // Molten Core's hounds only feign and rise when Instance.MoltenCoreHoundsLinked is on (default off: they die one by one).
        if (rule.enemy == 11671 && !sWorld->getBoolConfig(CONFIG_MC_CORE_HOUNDS_LINKED))
            continue;
        std::vector<Unit*> up;
        for (Unit* enemy : plan.enemies)
        {
            if (enemy->GetEntry() != rule.enemy)
                continue;
            if (enemy->GetHealth() <= 1 || (rule.downAura && enemy->HasAura(rule.downAura)))
                forbiddenAll.push_back(enemy->GetGUID());
            else
                up.push_back(enemy);
        }
        if (up.empty())
            continue;
        Unit* healthiest = *std::max_element(up.begin(), up.end(),
            [](Unit* a, Unit* b) { return a->GetHealthPct() < b->GetHealthPct(); });
        for (Unit* enemy : up)
            if (enemy->GetHealthPct() < healthiest->GetHealthPct() - rule.spreadPct)
                forbiddenDps.push_back(enemy->GetGUID());
        packHolders.push_back(healthiest);
        // The group kill focus too: the coordinator's generic pick (lowest health first) would chase a downed or trailing one.
        if (plan.ruleFocus.IsEmpty())
            plan.ruleFocus = healthiest->GetGUID();
    }
    auto const trailsHolder = [&packHolders](Unit* enemy)
    {
        for (Unit* holder : packHolders)
            if (holder->GetEntry() == enemy->GetEntry() && holder != enemy)
                return true;
        return false;
    };

    Unit* tankTarget = plan.mainTank ? plan.mainTank->GetVictim() : nullptr;
    std::vector<Unit*> candidates;   // enemies a member may be told to kill; the rank is per member (Priority::who / gate)
    for (Unit* enemy : plan.enemies)
    {
        if (std::find(forbiddenAll.begin(), forbiddenAll.end(), enemy->GetGUID()) != forbiddenAll.end() ||
            std::find(forbiddenDps.begin(), forbiddenDps.end(), enemy->GetGUID()) != forbiddenDps.end())
            continue;   // a downed or trailing pack member
        if (trailsHolder(enemy))
            continue;   // within the spread but not the healthiest: no focus on it
        bool const reflecting = enemy->HasAura(kMagicReflection) || enemy->HasAura(kDamageShield);
        // The owner's skull on a sheeped mob is the call to kill it (Majordomo: he rotates the skull onto each sheeped Healer).
        bool const skulled_hold = Controlled(enemy) && !coordFocus.IsEmpty() && enemy->GetGUID() == coordFocus;
        if ((Controlled(enemy) && !skulled_hold) || reflecting)
        {
            forbiddenAll.push_back(enemy->GetGUID());
            continue;
        }
        if (PriorityFor(rules, enemy->GetEntry()) < 0)
        {
            forbiddenDps.push_back(enemy->GetGUID());
            continue;
        }
        if (!enemy->GetVictim())
            continue; // unpulled: the tank owns first contact
        candidates.push_back(enemy);
    }
    // First matching entry per enemy wins; a gated entry needs a living gate enemy inside its health window.
    auto const priorityFor = [&](Member const& m, Unit* enemy)
    {
        for (Priority const& rule : rules.priority)
        {
            if (rule.enemy != enemy->GetEntry() || !Matches(rule.who, m, plan.mainTank))
                continue;
            if (rule.gateEnemy)
            {
                bool gated = false;
                for (Unit* gate : plan.enemies)
                    if (gate->GetEntry() == rule.gateEnemy && InPhase(gate, rule.hpMax, rule.hpMin))
                        gated = true;
                if (!gated)
                    continue;
            }
            return rule.value;
        }
        return 1;
    };
    auto const bestFor = [&](Member const& m) -> Unit*
    {
        Unit* best = nullptr;
        int bestPriority = 0;
        for (Unit* enemy : candidates)
        {
            int const priority = priorityFor(m, enemy);
            if (priority < 0)
                continue;
            if (!best || priority > bestPriority || (priority == bestPriority && enemy == tankTarget))
            {
                best = enemy;
                bestPriority = priority;
            }
        }
        return best;
    };
    for (Member const& m : plan.members)
    {
        if (!m.bot)
            continue;
        auto it = orders.find(m.p->GetGUID().GetCounter());
        if (it == orders.end())
            continue;
        RaidOrder& o = it->second;
        o.forbidden_targets.insert(o.forbidden_targets.end(), forbiddenAll.begin(), forbiddenAll.end());
        if (!m.tank)
            o.forbidden_targets.insert(o.forbidden_targets.end(), forbiddenDps.begin(), forbiddenDps.end());
        if (!m.tank)
            for (OnlyAttackers const& rule : rules.onlyAttackers)
                if (!Matches(rule.who, m, plan.mainTank))
                    for (Unit* enemy : plan.enemies)
                        if (enemy->GetEntry() == rule.enemy)
                            o.forbidden_targets.push_back(enemy->GetGUID());
        if (!m.tank && !m.healer)
        {
            // A DPS with no target goes to the raid's kill focus (the owner's skull / cross) before any rule's pick: nothing here
            // competes with what the owner marked.
            ObjectGuid const focus = coordFocus;
            Unit* pick = nullptr;
            if (!focus.IsEmpty())
                for (Unit* enemy : candidates)
                    if (enemy->GetGUID() == focus)
                        pick = enemy;
            if (!pick)
                pick = bestFor(m);
            if (pick)
                o.fallback_target = pick->GetGUID();
        }
        else if (m.tank)
        {
            // A tank whose enemy is airborne (Onyxia in phase 2) cannot hold it: the enemy is forbidden for it and it takes the best ground
            // target (the whelps, Priority) so they do not reach the healers. The main tank keeps its enemy while it is on the ground.
            Unit* victim = m.p->GetVictim();
            bool const airborne = victim && victim->IsFlying();
            if (m.p != plan.mainTank || airborne || !victim)
                if (Unit* best = bestFor(m))
                    if (!best->IsFlying())
                        o.tank_fallback_target = best->GetGUID();
            if (airborne)
                o.forbidden_targets.push_back(victim->GetGUID());
        }
    }
}

// Tank-first hold (port of ike3 MCRaidHoldDamage): DPS wait until a tank holds the enemy with a lead, then keep their
// threat under 90% of the tank's. Never while the enemy is already hitting a non-tank (self defence), and never longer than
// a few seconds for the opening lead, so a missing or idle tank cannot freeze the raid.
std::unordered_map<uint64_t, uint32_t> g_holdSince;   // (bot << 32 | enemy) -> first held (ms)
// The over-threat hold is bounded too: held at most kOverHoldMaxMs on one enemy unless the bot is past the hard limit (aggro moves at
// 110 % melee / 130 % ranged of the holder's threat; 105 % / 120 % leave a margin). The clock restarts after kOverHoldGapMs unheld.
struct OverHold { uint32_t first = 0, last = 0; };
std::unordered_map<uint64_t, OverHold> g_overHold;
std::unordered_map<uint64_t, OverHold> g_holdFight;   // bot -> when its opening wait of this fight began (one wait per fight, not per enemy)
constexpr uint32_t kOverHoldMaxMs = 3000, kOverHoldGapMs = 4000, kFightGapMs = 8000;

void PublishThreatHolds(Plan& plan, std::unordered_map<uint64_t, RaidOrder>& orders, uint32_t now)
{
    bool anyTank = false;
    for (Member const& m : plan.members)
        if (m.tank) anyTank = true;
    if (!anyTank)
        return;
    for (Member const& m : plan.members)
    {
        if (!m.bot || m.tank || m.healer)
            continue;
        auto it = orders.find(m.p->GetGUID().GetCounter());
        if (it == orders.end())
            continue;
        RaidOrder& o = it->second;
        Unit* target = m.p->GetVictim();
        if (!target && !o.fallback_target.IsEmpty())
            for (Unit* enemy : plan.enemies)
                if (enemy->GetGUID() == o.fallback_target)
                    target = enemy;
        if (!target || target->IsPlayer() || !target->IsAlive() || !target->CanHaveThreatList())
            continue;
        if (Unit* victim = target->GetVictim())
            if (Player* vp = victim->ToPlayer())
            {
                Member const* vm = FindMember(plan, vp);
                if (!vm || !vm->tank)
                    continue;   // it is already hitting a non-tank: everybody fights
            }
        float tankThreat = 0.0f;
        bool tankNear = false;
        for (Member const& t : plan.members)
            if (t.tank && t.p->GetDistance(target) < 50.0f)
            {
                tankNear = true;
                tankThreat = std::max(tankThreat, target->GetThreatManager().GetThreat(t.p));
            }
        if (!tankNear)
            continue;
        // The reference is the threat of whoever the enemy is on, not the best of all tanks: after a taunt swap or with a tank on an add
        // the maximum is a number the enemy's victim does not have.
        if (Unit* v = target->GetVictim())
            if (Player* vp = v->ToPlayer())
                if (Member const* vm = FindMember(plan, vp); vm && vm->tank)
                    tankThreat = target->GetThreatManager().GetThreat(vp);
        float myThreat = target->GetThreatManager().GetThreat(m.p);
        if (Pet* pet = m.p->GetPet())
            myThreat = std::max(myThreat, target->GetThreatManager().GetThreat(pet));
        Creature* creature = target->ToCreature();
        bool const boss = IsBoss(creature, plan.rules);
        float const minLead = boss ? 900.0f : 250.0f;
        bool hold = false;
        uint64_t const key = (m.p->GetGUID().GetCounter() << 32) | (target->GetGUID().GetCounter() & 0xFFFFFFFFull);
        if (tankThreat < minLead)
        {
            uint32_t& since = g_holdSince[key];
            if (!since || now - since > 120000)
                since = now;
            // One opening wait per fight: the clock of the bot's first held enemy counts for the next ones of the same pull.
            OverHold& fight = g_holdFight[m.p->GetGUID().GetCounter()];
            if (!fight.first || now - fight.last > kFightGapMs)
                fight.first = now;
            fight.last = now;
            // A boss gets the tank's time to arrive and build its lead: mc_geddon 09a8ff07 had Geddon on a mage 6 s in (the tank, 40 y off at the
            // pull, still short of the 900 lead when the 6 s opening wait ran out), loose in the middle of the raid when Inferno came.
            // Never longer than six seconds from the tank's first hit (playtest 2026-10-15), the same cap as the combat state's wait.
            uint32_t const capMs = boss ? 6000u : (tankThreat <= 0.0f ? 1000u : 2000u);
            hold = now - std::min(since, fight.first) < capMs;
        }
        else
        {
            // At the tank's threat, not under 90 % of it (24 DPS capped at about the tank's rate less a tenth: MC Garr 2026-10-10).
            // A boss that cuts its tank's threat on a schedule is held at what stays after a cut (RaidRules::threatDrop), like the generic hold.
            float const share = (boss && creature && plan.rules) ? ThreatShareAfterDrop(*plan.rules, creature->GetEntry()) : 1.0f;
            // Early in the pull the hold starts a typical hit short of the tank's threat and is not released until the tank has built up
            // (ThreatEarlyHoldRatio), the same as the generic hold.
            // A boss with a scheduled cut: ThreatEarlyHoldRatio already measures one more typical hit against the tank as it will be after the cut
            // (kPullSafe* x share), so the base is the normal 1.0, not the share too (n38 onyxia_full: the base and the release limit were both
            // exactly the share, so the timed release below could never apply, and DPS stood at 75-82 % of the tank for 13 s and more).
            bool const cut = share < 1.0f;
            float const holdRatio = ThreatEarlyHoldRatio(cut ? 1.0f : share, !m.ranged, tankThreat, share);
            hold = myThreat >= tankThreat * holdRatio;
            if (hold)
            {
                // The timed release reaches the ratio at which the bot is still at the safe pull ratio of the tank after the cut.
                float const hard = cut ? (m.ranged ? kPullSafeRanged : kPullSafeMelee) * share
                                       : std::min({ ThreatHardRatio(!m.ranged, /*raid=*/true, tankThreat), share,
                                                    holdRatio < share ? holdRatio : 1.0e9f });   // the same limits as the generic hold (Bot/ThreatLimits.h)
                OverHold& oh = g_overHold[key];
                if (!oh.first || now - oh.last > kOverHoldGapMs)
                    oh.first = now;
                oh.last = now;
                if (myThreat < tankThreat * hard && now - oh.first >= kOverHoldMaxMs)
                    hold = false;
            }
        }
        if (hold)
            o.hold_target = target->GetGUID();
    }
    if (g_holdSince.size() > 1024)
        for (auto it = g_holdSince.begin(); it != g_holdSince.end();)
            it = (now - it->second > 120000) ? g_holdSince.erase(it) : std::next(it);
    for (auto* map : { &g_overHold, &g_holdFight })
        if (map->size() > 1024)
            for (auto it = map->begin(); it != map->end();)
                it = (now - it->second.last > 120000) ? map->erase(it) : std::next(it);
}

// Which spell schools to prefer / avoid on each engaged enemy, from its live immunities, damage-taken auras and spell
// reflection (no table: the Death Talons' random vulnerability, the Plague Slime's colour and an Anubisath Sentinel's
// reflect all show up here, as does any other immunity).
std::vector<Brain::EnemySchools> SchoolHints(Plan const& plan, std::ostringstream& dump)
{
    std::vector<Brain::EnemySchools> out;
    for (Unit* enemy : plan.enemies)
    {
        // An evading creature reads immune to every school while it resets; that is no verdict on the mob (Garr 2026-10-08: the
        // raid held its damage on a Firesworn add that was only fire-immune). Banish stays: holding off a banished mob is the point.
        if (Creature const* c = enemy->ToCreature(); c && c->IsInEvadeMode())
            continue;
        uint8_t prefer = 0, avoid = 0;
        for (uint32_t school = SPELL_SCHOOL_MASK_NORMAL; school <= SPELL_SCHOOL_MASK_ARCANE; school <<= 1)
        {
            if (enemy->IsImmunedToDamage(SpellSchoolMask(school)))
            {
                avoid |= uint8_t(school);
                continue;
            }
            float const taken = enemy->GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN, school);
            if (taken >= 1.5f)
                prefer |= uint8_t(school);
            else if (taken <= 0.5f)
                avoid |= uint8_t(school);
        }
        // Spell reflection (an Anubisath Sentinel's Fire/Arcane or Shadow/Frost Reflect): those schools come back at the caster.
        for (AuraEffect const* eff : enemy->GetAuraEffectsByType(SPELL_AURA_REFLECT_SPELLS_SCHOOL))
            avoid |= uint8_t(eff->GetMiscValue() & SPELL_SCHOOL_MASK_MAGIC);
        if (enemy->HasAuraType(SPELL_AURA_REFLECT_SPELLS))
            avoid |= uint8_t(SPELL_SCHOOL_MASK_MAGIC);
        prefer &= uint8_t(~avoid);
        if (!prefer && !avoid)
            continue;
        // Why: the first sighting of an entry with this mask says which immunity, aura or reflect produced it (Garr 2026-10-09:
        // a Firesworn read immune to all seven schools and nothing in the log said from what).
        if (avoid)
        {
            static std::unordered_map<uint64_t, uint32_t> sWhyLogged;
            uint64_t const wkey = (uint64_t(enemy->GetEntry()) << 8) | avoid;
            uint32_t const wnow = GameTime::GetGameTimeMS();
            if (uint32_t& at = sWhyLogged[wkey]; !at || wnow - at > 60000)
            {
                at = wnow;
                std::ostringstream why;
                why << "school immunity 0x" << std::hex << enemy->GetSchoolImmunityMask() << " damage immunity 0x"
                    << enemy->GetDamageImmunityMask() << std::dec << "; auras:";
                for (AuraEffect const* eff : enemy->GetAuraEffectsByType(SPELL_AURA_SCHOOL_IMMUNITY))
                    why << " immune(" << eff->GetId() << ")";
                for (AuraEffect const* eff : enemy->GetAuraEffectsByType(SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN))
                    why << " taken(" << eff->GetId() << " mask 0x" << std::hex << eff->GetMiscValue() << std::dec << " "
                        << eff->GetAmount() << "%)";
                for (AuraEffect const* eff : enemy->GetAuraEffectsByType(SPELL_AURA_REFLECT_SPELLS_SCHOOL))
                    why << " reflect(" << eff->GetId() << ")";
                if (enemy->HasAuraWithMechanic(1u << MECHANIC_BANISH))
                    why << " banished";
                TC_LOG_INFO("playerbot.v2", "[raid_school_why] {} entry {} ({}): avoid 0x{:x} prefer 0x{:x} - {}",
                    enemy->GetName(), enemy->GetEntry(), enemy->GetGUID().GetCounter(), uint32_t(avoid), uint32_t(prefer), why.str());
            }
        }
        out.push_back({ enemy->GetGUID(), prefer, avoid });
        dump << "  schools " << enemy->GetName() << " (" << enemy->GetGUID().GetCounter() << "): prefer 0x" << std::hex
             << uint32_t(prefer) << " avoid 0x" << uint32_t(avoid) << std::dec << "\n";
    }
    return out;
}

} // namespace detail

using namespace detail;

Brain& GetBrain()
{
    static Brain brain;
    return brain;
}

namespace {

// Pull log for the test-night digest (scripts/bot-log-digest.ps1). One [raid_encounter] line when a group on a raid map
// enters combat and one when the fight is over (no enemy for 4 s): what was pulled, how long it took and how it ended.
// World thread only, a few map operations per raid group every plan pass.
struct PullLog
{
    bool active = false;
    uint32_t mapId = 0, instanceId = 0;
    uint64_t groupLow = 0;
    uint32_t startMs = 0, lastSeenMs = 0;
    uint32_t startAlive = 0, groupSize = 0;
    uint32_t aliveLast = 0;   // raiders alive at the last plan pass of the pull (the end line is written 4 s later, often after the squad left the map)
    // seenDead: its corpse was seen. A mob that merely left the map (evade and despawn after a wipe) is not a kill (n32 mc_majordomo: the
    // eight adds despawned at 89-99 % health and the line read "killed=8/9").
    struct Mob { uint32_t entry; std::string name; bool boss; bool seenDead = false; int32_t lastPct = 100; };
    std::map<ObjectGuid, Mob> engaged;
    bool readyLogged = false;
    // Deaths in this pull, for the wipe report: who fell first and what was hitting them.
    std::unordered_map<uint64_t, bool> wasAlive;
    std::unordered_map<uint64_t, std::string> hitBy;
    std::string firstDead, firstCause;
    uint32_t deaths = 0;
};
std::unordered_map<uint64_t, PullLog> g_pulls;
constexpr uint32_t kPullEndQuietMs = 4000;

std::string PullLabel(PullLog const& pl)
{
    std::map<std::string, uint32_t> count;
    std::string boss;
    for (auto const& [guid, mob] : pl.engaged)
    {
        if (mob.boss && boss.empty())
            boss = mob.name;
        ++count[mob.name];
    }
    std::ostringstream out;
    if (!boss.empty())
        out << "boss " << boss << " | ";
    bool first = true;
    for (auto const& [name, n] : count)
    {
        out << (first ? "" : ", ") << name;
        if (n > 1)
            out << " x" << n;
        first = false;
    }
    return out.str();
}

// Boss readiness (Molten Core, Onyxia, Blackwing Lair): what the encounter asks of the raid's classes against who is actually in the raid, so a
// missing curse dispel, tranquilizer or banisher shows up in the digest before the wipe it causes. Counts are alive
// members on the map; the needs are the rules in RaidEncounterData_MC.cpp (dispels, purges, banish, polymorph, kicks).
constexpr uint32_t ClassBit(uint8_t cls) { return 1u << cls; }
constexpr uint32_t kCurseCleaners = ClassBit(CLASS_MAGE) | ClassBit(CLASS_DRUID);
constexpr uint32_t kMagicCleaners = ClassBit(CLASS_PRIEST) | ClassBit(CLASS_PALADIN);
constexpr uint32_t kPurgers = ClassBit(CLASS_PRIEST) | ClassBit(CLASS_SHAMAN);
constexpr uint32_t kTranquilizers = ClassBit(CLASS_HUNTER);
constexpr uint32_t kFearCover = ClassBit(CLASS_PRIEST) | ClassBit(CLASS_SHAMAN);
constexpr uint32_t kBanishers = ClassBit(CLASS_WARLOCK);
constexpr uint32_t kPolymorphers = ClassBit(CLASS_MAGE);
constexpr uint32_t kPoisonCleaners = ClassBit(CLASS_DRUID) | ClassBit(CLASS_SHAMAN) | ClassBit(CLASS_PALADIN);
constexpr uint32_t kDiseaseCleaners = ClassBit(CLASS_PRIEST) | ClassBit(CLASS_SHAMAN) | ClassBit(CLASS_PALADIN);
constexpr uint32_t kAoeCasters = ClassBit(CLASS_MAGE) | ClassBit(CLASS_WARLOCK);
constexpr uint32_t kKickers = ClassBit(CLASS_ROGUE) | ClassBit(CLASS_WARRIOR) | ClassBit(CLASS_MAGE) | ClassBit(CLASS_SHAMAN);
constexpr uint32_t kPriests = ClassBit(CLASS_PRIEST);
constexpr uint32_t kMages = ClassBit(CLASS_MAGE);

struct BossNeed { uint32_t boss; char const* what; uint32_t classes; uint32_t want; };
constexpr BossNeed kBossNeeds[] = {
    { 12118, "curse dispel (Lucifron's Curse)", kCurseCleaners, 2 },
    { 12118, "magic dispel (Impending Doom)", kMagicCleaners, 2 },
    { 12118, "polymorph (Dominate Mind)", kPolymorphers, 1 },
    { 11982, "tranquilizing shot (Frenzy)", kTranquilizers, 1 },
    { 11982, "fear cover (Panic)", kFearCover, 1 },
    { 12259, "curse dispel (Gehennas' Curse)", kCurseCleaners, 2 },
    { 12057, "banish (Firesworn)", kBanishers, 2 },
    { 12264, "curse dispel (Shazzrah's Curse)", kCurseCleaners, 2 },
    { 12264, "purge (Deaden Magic)", kPurgers, 1 },
    { 12098, "interrupt (Dark Mending)", kKickers, 3 },
    { 12098, "purge (Inspire)", kPurgers, 1 },
    { 12018, "polymorph (Flamewaker Healers)", kPolymorphers, 2 },
    { 12018, "purge (Magic Reflection / Damage Shield)", kPurgers, 2 },
    { 12018, "interrupt (Flamewaker Healers)", kKickers, 3 },
    { 11502, "banish (Sons of Flame)", kBanishers, 2 },
    // Onyxia's Lair
    { 10184, "fear cover (Bellowing Roar)", kFearCover, 1 },
    { 10184, "area damage (whelps)", kAoeCasters, 2 },
    // Blackwing Lair
    { 12435, "polymorph (Grethok's Dominate Mind)", kPolymorphers, 1 },
    { 13020, "tranquilizing shot (Seether Frenzy)", kTranquilizers, 1 },
    { 11981, "tranquilizing shot (Flamegor Frenzy)", kTranquilizers, 1 },
    { 14020, "magic dispel (Bronze / Blue Affliction)", kMagicCleaners, 2 },
    { 14020, "curse dispel (Black Affliction)", kCurseCleaners, 1 },
    { 14020, "disease cleanse (Red Affliction)", kDiseaseCleaners, 1 },
    { 14020, "poison cleanse (Green Affliction)", kPoisonCleaners, 1 },
    { 14020, "tranquilizing shot (Frenzy)", kTranquilizers, 1 },
    { 11583, "fear cover (Bellowing Roar)", kFearCover, 1 },
    { 11583, "magic dispel (Polymorph call)", kMagicCleaners, 2 },
    // Temple of Ahn'Qiraj
    { 15263, "interrupt (Arcane Explosion)", kKickers, 2 },
    { 15263, "polymorph (True Fulfillment)", kPolymorphers, 1 },
    { 15543, "interrupt (Yauj's Heal)", kKickers, 2 },
    { 15543, "fear cover (Yauj's Fear)", kFearCover, 1 },
    { 15543, "poison cleanse (Kri's Toxic Volley)", kPoisonCleaners, 2 },
    { 15299, "frost damage (freezing Viscidus)", kMages, 3 },
    { 15299, "poison cleanse (Poison Bolt Volley)", kPoisonCleaners, 2 },
    { 15509, "tranquilizing shot (Frenzy)", kTranquilizers, 1 },
    { 15509, "poison cleanse (Noxious Poison)", kPoisonCleaners, 2 },
    { 15276, "casters for Vek'lor (immune to physical)", kAoeCasters, 4 },
    // Naxxramas
    { 15953, "poison cleanse (Poison Bolt Volley)", kPoisonCleaners, 2 },
    { 15953, "priests (worshipper mind control)", kPriests, 2 },
    { 15952, "poison cleanse (Necrotic Poison)", kPoisonCleaners, 2 },
    { 15954, "curse dispel (Curse of the Plaguebringer)", kCurseCleaners, 2 },
    { 15936, "disease cleanse (Decrepit Fever)", kDiseaseCleaners, 2 },
    { 16061, "priests (mind control the understudies)", kPriests, 2 },
    { 15932, "tranquilizing shot (Frenzy)", kTranquilizers, 1 },
    { 15989, "curse dispel (Life Drain)", kCurseCleaners, 2 },
    { 15990, "shackle (Guardians of Icecrown)", kPriests, 3 },
    { 15990, "polymorph (Chains of Kel'Thuzad)", kPolymorphers, 1 },
};

struct BossReadiness
{
    bool any = false;
    std::string have, gaps;
    std::vector<NeedGap> missing;
    uint32_t tanks = 0, healers = 0;
    int healerManaPct = -1;   // average mana of the healers that have a mana bar, -1 = none
};

// Fear cover: a shaman who knows Tremor Totem or a priest who knows Fear Ward (a priest of the wrong race does not).
constexpr uint32_t kTremorTotem = 8143, kFearWard = 6346;
bool CoversFear(Player* p)
{
    return (p->GetClass() == CLASS_SHAMAN && p->HasSpell(kTremorTotem)) || (p->GetClass() == CLASS_PRIEST && p->HasSpell(kFearWard));
}

BossReadiness EvaluateBoss(uint32_t bossEntry, detail::Plan const& plan)
{
    BossReadiness r;
    for (BossNeed const& need : kBossNeeds)
    {
        if (need.boss != bossEntry)
            continue;
        r.any = true;
        uint32_t n = 0;
        bool const fear = need.classes == kFearCover;
        for (detail::Member const& m : plan.members)
            if (m.p && (need.classes & ClassBit(m.p->GetClass())) && (!fear || CoversFear(m.p)))
                ++n;
        r.have += (r.have.empty() ? "" : ", ") + std::string(need.what) + " " + std::to_string(n) + "/" + std::to_string(need.want);
        if (n < need.want)
        {
            r.gaps += (r.gaps.empty() ? "" : ", ") + std::string(need.what) + " " + std::to_string(n) + "/" + std::to_string(need.want);
            r.missing.push_back({ need.what, n, need.want, need.classes, fear ? "Tremor Totem shaman or Fear Ward priest" : "" });
        }
    }
    if (!r.any)
        return r;
    uint32_t manaSum = 0, manaHealers = 0;
    for (detail::Member const& m : plan.members)
    {
        r.tanks += m.tank ? 1 : 0;
        r.healers += m.healer ? 1 : 0;
        if (m.healer && m.p && m.p->GetMaxPower(POWER_MANA) > 0)
        {
            manaSum += uint32_t(uint64_t(m.p->GetPower(POWER_MANA)) * 100 / uint64_t(m.p->GetMaxPower(POWER_MANA)));
            ++manaHealers;
        }
    }
    if (manaHealers)
        r.healerManaPct = int(manaSum / manaHealers);
    // Raid make-up: one healer per five raiders, three tanks from 25 up. Fewer is a wipe waiting to happen
    // (Gehennas 2026-10-08: 5 healers for 40, tanks at 38% after 6 s).
    if (plan.members.size() >= 25)
    {
        uint32_t const needHealers = uint32_t(plan.members.size() + 4) / 5;
        if (r.healers < needHealers)
        {
            r.gaps += (r.gaps.empty() ? "" : ", ") + std::string("healers ") + std::to_string(r.healers) + "/" + std::to_string(needHealers);
            r.missing.push_back({ "healers", r.healers, needHealers, 0u });
        }
        if (r.tanks < 3)
        {
            r.gaps += (r.gaps.empty() ? "" : ", ") + std::string("tanks ") + std::to_string(r.tanks) + "/3";
            r.missing.push_back({ "tanks", r.tanks, 3u, 0u });
        }
    }
    return r;
}

// The gaps to the humans of the group (playtest, MC Magmadar 2026-10-10: no hunter in the raid, nobody could remove Frenzy and only
// the log said so). Once per boss per minute, whether it is sent before the pull or at the engage.
std::unordered_map<uint64_t, uint32_t> g_readyWarned;   // (group << 32 | boss entry) -> ms of the last warning

void WarnOwners(Map* map, uint64_t groupLow, uint32_t bossEntry, std::string const& bossName, std::vector<NeedGap> const& missing,
                uint32_t now_ms, int healerManaPct = -1)
{
    std::string const warning = ReadyWarning(bossName, missing, healerManaPct);
    if (warning.empty() || !map)
        return;
    uint32_t& last = g_readyWarned[(groupLow << 32) ^ bossEntry];
    if (!ReadyWarnDue(last, now_ms))
        return;
    last = now_ms ? now_ms : 1u;
    for (MapReference const& ref : map->GetPlayers())
        if (Player* hp = ref.GetSource(); hp && hp->GetGroup() && hp->GetGroup()->GetGUID().GetCounter() == groupLow &&
            hp->GetSession() && !Services::Lifecycle().is_bot(hp->GetGUID().GetCounter()))
            ChatHandler(hp->GetSession()).SendSysMessage(warning);
}

// Before the pull: a human member targets a boss that is not in combat yet ("pull" and "attack" act on the owner's target, so
// they are covered), and the raid lacks something the boss asks for.
void WarnBeforePull(detail::Plan const& plan, Map* map, uint32_t now_ms)
{
    for (detail::Member const& m : plan.members)
    {
        if (m.bot || !m.p)
            continue;
        Unit* t = ObjectAccessor::GetUnit(*m.p, m.p->GetTarget());
        if (!t || t->IsPlayer() || !t->IsAlive() || t->IsInCombat() || t->GetMap() != m.p->GetMap() || m.p->GetDistance(t) > 100.0f)
            continue;
        BossReadiness const r = EvaluateBoss(t->GetEntry(), plan);
        if (!r.any)
            continue;
        // A human pull skips the bots' mana gate (the owner pulled Magmadar with the healers at 58%), so the mana is checked here too.
        WarnOwners(map, plan.groupLow, t->GetEntry(), t->GetName(), r.missing, now_ms, r.healerManaPct);
        return;
    }
}

void LogBossReadiness(PullLog const& pl, detail::Plan const& plan, Map* map, uint32_t now_ms)
{
    for (auto const& [guid, mob] : pl.engaged)
    {
        if (!mob.boss)
            continue;
        BossReadiness const r = EvaluateBoss(mob.entry, plan);
        if (!r.any)
            continue;
        TC_LOG_INFO("playerbot.v2.raid", "[raid_encounter] ts={} map={} group={} ready boss {}: tanks {} healers {} | {} | {}{}",
            uint64_t(GameTime::GetGameTime()), pl.mapId, pl.groupLow, mob.name, r.tanks, r.healers, r.have,
            r.gaps.empty() ? "no gaps" : "GAPS: ", r.gaps);
        WarnOwners(map, pl.groupLow, mob.entry, mob.name, r.missing, now_ms);
        return;
    }
}

void TrackPull(uint64_t key, Group* group, detail::Plan const& plan, Map* map, uint32_t now_ms)
{
    if (plan.enemies.empty())
        return;
    PullLog& pl = g_pulls[key];
    if (!pl.active)
    {
        pl = PullLog{};
        pl.active = true;
        pl.mapId = plan.mapId;
        pl.instanceId = map->GetInstanceId();
        pl.groupLow = plan.groupLow;
        pl.startMs = now_ms;
        pl.startAlive = uint32_t(plan.members.size());
        pl.groupSize = group->GetMembersCount();
        ResetHealStats();
        ManaLedgerReset();
        IncomingReset();
    }
    pl.lastSeenMs = now_ms;
    pl.aliveLast = uint32_t(plan.members.size());
    for (Member const& m : plan.members)
        IncomingWatch(m.p->GetGUID().GetCounter(), m.tank);
    for (GroupReference const& gref : group->GetMembers())
    {
        Player* gp = gref.GetSource();
        if (!gp || !gp->IsInWorld() || gp->GetMap() != map)
            continue;
        uint64_t const gl = gp->GetGUID().GetCounter();
        bool const alive = gp->IsAlive();
        // The mana ledger watches every class that can heal; it prints only those that sent a heal.
        if (alive)
            if (uint8_t const gc = gp->GetClass(); gc == CLASS_PRIEST || gc == CLASS_PALADIN || gc == CLASS_DRUID || gc == CLASS_SHAMAN)
                ManaLedgerSample(gl, gp->GetName(), uint32(gp->GetPower(POWER_MANA)), uint32(gp->GetMaxPower(POWER_MANA)), GameTime::GetGameTimeMS());
        if (auto w = pl.wasAlive.find(gl); w != pl.wasAlive.end() && w->second && !alive)
        {
            ++pl.deaths;
            if (pl.firstDead.empty())
            {
                pl.firstDead = gp->GetName();
                auto h = pl.hitBy.find(gl);
                pl.firstCause = h != pl.hitBy.end() && !h->second.empty() ? h->second : std::string("no mob on them");
            }
        }
        pl.wasAlive[gl] = alive;
        std::string hit;
        for (Unit* u : plan.enemies)
            if (u->GetVictim() == gp)
                hit += (hit.empty() ? "" : ", ") + u->GetName();
        pl.hitBy[gl] = std::move(hit);
    }
    bool added = false;
    for (Unit* u : plan.enemies)
    {
        Creature* c = u->ToCreature();
        if (!c || pl.engaged.count(c->GetGUID()))
            continue;
        bool const boss = detail::IsBoss(c, plan.rules);
        pl.engaged.emplace(c->GetGUID(), PullLog::Mob{ c->GetEntry(), c->GetName(), boss });
        added = true;
    }
    for (auto& [eg, em] : pl.engaged)
        if (Creature* ec = map->GetCreature(eg))
        {
            if (!ec->IsAlive())
                em.seenDead = true;
            else
                em.lastPct = int32_t(ec->GetHealthPct());
        }
    if (added && now_ms == pl.startMs)
        TC_LOG_INFO("playerbot.v2.raid", "[raid_encounter] ts={} map={} group={} pull start: {} | alive={}/{}",
            uint64_t(GameTime::GetGameTime()), pl.mapId, pl.groupLow, PullLabel(pl), pl.startAlive, pl.groupSize);
    if (!pl.readyLogged && !pl.engaged.empty())
    {
        bool bossIn = false;
        for (auto const& [guid, mob] : pl.engaged)
            bossIn = bossIn || mob.boss;
        if (bossIn)
        {
            pl.readyLogged = true;
            LogBossReadiness(pl, plan, map, now_ms);
        }
    }
}

void EndQuietPulls(uint32_t now_ms)
{
    for (auto it = g_pulls.begin(); it != g_pulls.end();)
    {
        PullLog& pl = it->second;
        if (!pl.active || now_ms - pl.lastSeenMs < kPullEndQuietMs)
        {
            ++it;
            continue;
        }
        Map* map = sMapMgr->FindMap(pl.mapId, pl.instanceId);
        uint32_t killed = 0, bosses = 0, bossesDead = 0;
        std::string left;
        for (auto const& [guid, mob] : pl.engaged)
        {
            Creature* c = map ? map->GetCreature(guid) : nullptr;
            // Gone from the map is a despawn, not a kill, unless its corpse was seen or it was last seen at the edge of death: a boss or an
            // add that vanished at 90 % was reset.
            bool const dead = mob.seenDead || (c && !c->IsAlive()) || (!c && mob.lastPct <= 3);
            if (mob.boss)
            {
                ++bosses;
                bossesDead += dead;
            }
            if (dead)
                ++killed;
            else if (left.size() < 120)
                left += (left.empty() ? "" : ", ") + mob.name + " " + std::to_string(c ? int32(c->GetHealthPct()) : mob.lastPct) + "%";
        }
        // The raiders alive at the last plan pass of the pull: by the time this line is written the squad may be out of the map (a scenario
        // that ends takes it out), which read as alive=0/40 on every clean kill (n24 2026-10-14).
        uint32_t alive = pl.aliveLast;
        // A human of the group lying dead on the map with no living raider left on it is a wipe, however many bots stood at the last plan
        // pass or died before the squad was counted out (n58 live 2026-10-15: 29 deaths of 40, 13 "alive" from the last pass, result "reset":
        // the encounter end below never ran and Release Spirit stayed greyed out, "a member of your group is in combat").
        bool humanDeadNoneAlive = false;
        if (map)
        {
            uint32_t onMap = 0;
            bool humanDead = false;
            for (MapReference const& ref : map->GetPlayers())
                if (Player* p = ref.GetSource(); p && p->GetGroup() && p->GetGroup()->GetGUID().GetCounter() == pl.groupLow)
                {
                    if (p->IsAlive())
                        ++onMap;
                    else if (!Services::Lifecycle().is_bot(p->GetGUID().GetCounter()))
                        humanDead = true;
                }
            if (onMap)
                alive = onMap;
            else if (humanDead)
            {
                humanDeadNoneAlive = true;
                alive = 0;
            }
        }
        // A wipe is raiders dead, not raiders gone: a scenario that ends at its death limit takes the squad out of the map, and a raid that
        // left the instance is no wipe either (onyxia_full / mc_geddon n7 2026-10-14: six deaths, run over, "alive=0/40 wipe").
        bool const mostDead = pl.deaths * 4 >= pl.startAlive * 3;
        // The boss dead is a kill even with its adds still standing (Garr's Firesworn, Ragnaros' Sons of Flame): the left list names them.
        bool const bossKilled = bosses > 0 && bossesDead == bosses;
        char const* result = killed == pl.engaged.size() || bossKilled ? "kill" : ((alive * 4 <= pl.startAlive && mostDead) || humanDeadNoneAlive ? "wipe" : "reset");
        TC_LOG_INFO("playerbot.v2.raid", "[raid_encounter] ts={} map={} group={} pull end: result={} dur={}s killed={}/{} alive={}/{} deaths={} | {}{}{}",
            uint64_t(GameTime::GetGameTime()), pl.mapId, pl.groupLow, result, (pl.lastSeenMs - pl.startMs) / 1000, killed,
            uint32_t(pl.engaged.size()), alive, pl.startAlive, pl.deaths, PullLabel(pl), left.empty() ? "" : " | left: ", left);
        // Healer throughput of the pull: heal casts per minute per healer (a healer missing from the list cast nothing).
        uint32_t healersCast = 0;
        std::string const healStats = TakeHealStats(uint32_t((pl.lastSeenMs - pl.startMs) / 1000), healersCast);
        TC_LOG_INFO("playerbot.v2.raid", "[raid_fight] map={} group={} healers that cast: {} | {}", pl.mapId, pl.groupLow, healersCast,
            healStats.empty() ? "none" : healStats);
        for (std::string const& line : ManaLedgerTake(uint32_t((pl.lastSeenMs - pl.startMs) / 1000), [](uint32 id)
            {
                SpellInfo const* si = sSpellMgr->GetSpellInfo(id, DIFFICULTY_NONE);
                char const* const name = si && si->SpellName ? (*si->SpellName)[sWorld->GetDefaultDbcLocale()] : nullptr;
                return std::string(name && *name ? name : "spell") + "#" + std::to_string(id);
            }))
            TC_LOG_INFO("playerbot.v2.raid", "[mana_ledger] map={} group={} {}", pl.mapId, pl.groupLow, line);
        for (std::string const& line : DealtTake(uint32_t((pl.lastSeenMs - pl.startMs) / 1000)))
            TC_LOG_INFO("playerbot.v2.raid", "[raid_dealt] map={} group={} {}", pl.mapId, pl.groupLow, line);
        for (std::string const& line : IncomingTake(uint32_t((pl.lastSeenMs - pl.startMs) / 1000)))
            TC_LOG_INFO("playerbot.v2.raid", "[raid_incoming] map={} group={} {}", pl.mapId, pl.groupLow, line);
        // Wipe report to the humans of the group (a system line in their chat window).
        if (std::strcmp(result, "wipe") == 0)
            if (map)
                for (MapReference const& ref : map->GetPlayers())
                    if (Player* hp = ref.GetSource(); hp && hp->GetGroup() && hp->GetGroup()->GetGUID().GetCounter() == pl.groupLow &&
                        hp->GetSession() && !Services::Lifecycle().is_bot(hp->GetGUID().GetCounter()))
                        ChatHandler(hp->GetSession()).SendSysMessage(fmt::format("Wipe: {} lasted {} s, {} died, first down {}.",
                            PullLabel(pl), (pl.lastSeenMs - pl.startMs) / 1000, pl.deaths,
                            pl.firstDead.empty() ? std::string("nobody seen") : pl.firstDead));
        // [raid_wipe] encounter end (playtest, MC Garr 2026-10-08: the whole raid dead and released, "A member of your group is in combat"
        // still blocked his release; the client's own encounter state stayed on). With nobody of the group alive on the map: end the
        // encounter for every group member, wherever they are (SendToPlayers reaches only the map), fail any boss still IN_PROGRESS
        // and make the mobs of the pull evade and reset.
        if (std::strcmp(result, "wipe") == 0 && alive == 0 && map)
        {
            uint32_t sent = 0, evaded = 0, failed = 0;
            if (Group const* wg = sGroupMgr->GetGroupByGUID(ObjectGuid::Create<HighGuid::Party>(pl.groupLow)))
                for (GroupReference const& gref : wg->GetMembers())
                    if (Player* gp = gref.GetSource(); gp && gp->GetSession())
                    {
                        WorldPackets::Instance::InstanceEncounterEnd endPkt;
                        gp->GetSession()->SendPacket(endPkt.Write());
                        ++sent;
                    }
            if (InstanceMap* im = map->ToInstanceMap())
              if (InstanceScript* script = im->GetInstanceScript())
                for (uint32 id = 0; id < script->GetEncounterCount(); ++id)
                    if (script->GetBossState(id) == IN_PROGRESS && script->SetBossState(id, FAIL))
                        ++failed;
            for (auto const& [guid, mob] : pl.engaged)
                if (Creature* c = map->GetCreature(guid); c && c->IsAlive() && c->IsInCombat() && c->AI())
                {
                    c->AI()->EnterEvadeMode(EvadeReason::NoHostiles);
                    ++evaded;
                }
            TC_LOG_INFO("playerbot.v2.raid", "[raid_wipe] encounter end: map={} group={} nobody alive: end sent to {} member(s), {} boss state(s) failed, {} mob(s) sent to evade",
                pl.mapId, pl.groupLow, sent, failed, evaded);
        }
        it = g_pulls.erase(it);
    }
}

} // namespace

// [release] (playtest, MC 2026-10-08: "A member of your group is in combat" blocks Release Spirit, which worked in the morning): while a human of
// the group is dead, every 5 s which members the client can see in combat, whether an offer is pending on him and whether the map's
// encounter is on. The server's repop handler has no combat check; this shows what the client is reacting to.
static void LogReleaseState(Group const* group, Map const* map, uint32_t now_ms)
{
    if (!group || !map)
        return;
    static std::unordered_map<uint64_t, uint32_t> last;
    for (GroupReference const& ref : group->GetMembers())
    {
        Player* h = ref.GetSource();
        if (!h || h->IsAlive() || !h->IsInWorld() || h->GetMap() != map || Services::Lifecycle().is_bot(h->GetGUID().GetCounter()))
            continue;
        uint32_t& l = last[h->GetGUID().GetCounter()];
        if (l && now_ms - l < 5000u)
            continue;
        l = now_ms ? now_ms : 1u;
        std::string combat;
        uint32_t n = 0;
        for (GroupReference const& oref : group->GetMembers())
            if (Player* o = oref.GetSource(); o && o != h && o->IsAlive() && o->IsInCombat())
            {
                ++n;
                if (combat.size() < 120)
                    combat += (combat.empty() ? "" : ", ") + o->GetName() + (o->GetMapId() == map->GetId() ? "" : "@other_map");
            }
        InstanceMap const* im = map->ToInstanceMap();
        InstanceScript const* script = im ? im->GetInstanceScript() : nullptr;
        TC_LOG_INFO("playerbot.v2.raid", "[release] {} is dead (ghost={} offer_pending={} self_in_combat={}): {} group member(s) alive and in combat: {}; encounter in progress: {}",
                    h->GetName(), int(h->HasPlayerFlag(PLAYER_FLAGS_GHOST)), int(h->IsResurrectRequested()), int(h->IsInCombat()), n,
                    combat.empty() ? "-" : combat, script ? int(script->IsEncounterInProgress()) : -1);
    }
}

// An encounter nobody is alive to fight is over (Onyxia 2026-10-15: the pull was judged at the instant one bot still lived, the wipe end
// above never ran, and the boss stayed IN_PROGRESS with the whole raid dead). With a human of a dungeon group dead and no living player
// left on his instance for 3 s: fail the bosses still IN_PROGRESS (that sends the encounter end to the client) and send the mobs in
// combat to evade.
static void EndAbandonedEncounters(uint32_t now_ms)
{
    static std::unordered_map<uint32_t, uint32_t> since;
    std::unordered_set<uint32_t> seen;
    for (auto const& [guid, h] : ObjectAccessor::GetPlayers())
    {
        if (!h || !h->IsInWorld() || h->IsAlive() || Services::Lifecycle().is_bot(h->GetGUID().GetCounter()))
            continue;
        Map* map = h->GetMap();
        InstanceMap* im = map ? map->ToInstanceMap() : nullptr;
        InstanceScript* script = im ? im->GetInstanceScript() : nullptr;
        if (!script || !script->IsEncounterInProgress() || !seen.insert(im->GetInstanceId()).second)
            continue;
        bool anyAlive = false;
        for (MapReference const& ref : im->GetPlayers())
            if (Player* p = ref.GetSource(); p && p->IsAlive() && !p->IsGameMaster())
            {
                anyAlive = true;
                break;
            }
        uint32_t& s = since[im->GetInstanceId()];
        if (anyAlive)
        {
            s = 0;
            continue;
        }
        if (!s)
        {
            s = now_ms ? now_ms : 1u;
            continue;
        }
        if (now_ms - s < 3000u)
            continue;
        s = 0;
        uint32_t failed = 0, evaded = 0;
        for (uint32 id = 0; id < script->GetEncounterCount(); ++id)
            if (script->GetBossState(id) == IN_PROGRESS && script->SetBossState(id, FAIL))
                ++failed;
        for (auto const& [spawnId, c] : im->GetCreatureBySpawnIdStore())
            if (c && c->IsInWorld() && c->IsAlive() && c->IsInCombat() && !c->IsControlledByPlayer() && c->AI())
            {
                c->AI()->EnterEvadeMode(EvadeReason::NoHostiles);
                ++evaded;
            }
        TC_LOG_INFO("playerbot.v2.raid", "[raid_wipe] encounter abandoned: map={} instance={} nobody alive on it: {} boss state(s) failed, {} mob(s) sent to evade",
            im->GetId(), im->GetInstanceId(), failed, evaded);
    }
}

// [release_state] (playtest 2026-10-15, MC Majordomo wipe: Release Spirit stayed greyed out, "a member of your group is in combat", for
// about two minutes after every bot had left the instance and the bosses had reset; [release] above stops once the raid is off the map).
// While a human of a dungeon group lies dead, every 5 s and wherever the raid is: his own combat flags, every group member's combat and
// pet-combat flags on any map, the creatures of his map still in combat, and the map's boss states. The first line names what is left.
static void LogDeadHumanReleaseState(uint32_t now_ms)
{
    static std::unordered_map<uint64_t, uint32_t> last;
    for (auto const& [guid, h] : ObjectAccessor::GetPlayers())
    {
        if (!h || !h->IsInWorld() || h->IsAlive() || Services::Lifecycle().is_bot(h->GetGUID().GetCounter()))
            continue;
        Group const* group = h->GetGroup();
        Map const* map = h->GetMap();
        if (!group || !map || !map->IsDungeon())
            continue;
        uint32_t& l = last[h->GetGUID().GetCounter()];
        if (l && now_ms - l < 5000u)
            continue;
        l = now_ms ? now_ms : 1u;

        uint32_t members = 0, onMap = 0, dead = 0, combat = 0, petCombat = 0, otherMap = 0;
        std::string names;
        for (GroupReference const& ref : group->GetMembers())
        {
            Player const* m = ref.GetSource();
            if (!m || m == h)
                continue;
            ++members;
            if (m->IsInWorld() && m->GetMap() == map)
                ++onMap;
            else
                ++otherMap;
            if (!m->IsAlive())
                ++dead;
            bool const c = m->IsInCombat(), pc = m->HasUnitFlag(UNIT_FLAG_PET_IN_COMBAT);
            if (c)
                ++combat;
            if (pc)
                ++petCombat;
            if ((c || pc) && names.size() < 160)
                names += (names.empty() ? "" : ", ") + m->GetName() + "[map " + std::to_string(m->GetMapId()) + (m->IsAlive() ? "" : " dead") +
                         (c ? " combat" : "") + (pc ? " pet" : "") + "]";
        }

        uint32_t mobsCombat = 0;
        std::string mobNames;
        for (auto const& [spawnId, c] : map->GetCreatureBySpawnIdStore())
            if (c && c->IsInWorld() && c->IsAlive() && c->IsInCombat() && !c->IsControlledByPlayer())
            {
                ++mobsCombat;
                if (mobNames.size() < 160)
                    mobNames += (mobNames.empty() ? "" : ", ") + c->GetName() + (c->HasUnitState(UNIT_STATE_EVADE) ? "[evade]" : "");
            }

        InstanceScript const* script = map->ToInstanceMap() ? map->ToInstanceMap()->GetInstanceScript() : nullptr;
        std::string bosses;
        if (script)
            for (uint32 id = 0; id < script->GetEncounterCount(); ++id)
                if (script->GetBossState(id) != NOT_STARTED)
                    bosses += (bosses.empty() ? "" : ", ") + std::to_string(id) + ":" + std::to_string(int(script->GetBossState(id)));

        TC_LOG_INFO("playerbot.v2.raid",
            "[release_state] {} dead map={} ghost={} self_combat={} self_pet_combat={} offer_pending={} | group {} others: {} on map, {} elsewhere, {} dead, {} in combat, {} pet in combat: {} | creatures in combat on map: {}: {} | encounter in progress: {} boss states (id:state, not NOT_STARTED): {}",
            h->GetName(), h->GetMapId(), int(h->HasPlayerFlag(PLAYER_FLAGS_GHOST)), int(h->IsInCombat()), int(h->HasUnitFlag(UNIT_FLAG_PET_IN_COMBAT)),
            int(h->IsResurrectRequested()), members, onMap, otherMap, dead, combat, petCombat, names.empty() ? "-" : names, mobsCombat,
            mobNames.empty() ? "-" : mobNames, script ? int(script->IsEncounterInProgress()) : -1, bosses.empty() ? "-" : bosses);
    }
}

// A resurrection a group bot cast on a human member sits as a pending offer until the client answers. The 70205 client has shown
// no "X wants to resurrect you" popup (MC 2026-10-08: the first healer's Resurrection landed, the offer stayed pending, every
// other rezzer saw the corpse as being raised and the owner stayed dead). A pending offer from a bot of the group is accepted
// server-side after a short grace, the way the client's Accept would (Playerbot.Raid.AutoAcceptOwnerRez).
static void AcceptBotRezOffers(Group const* group, Map const* map, uint32_t now_ms)
{
    if (!group || !Services::Config().raid_auto_accept_rez())
        return;
    static std::unordered_map<uint64_t, uint32_t> firstSeen;
    // The plan's member list holds the living only; the dead member waiting for the offer is read from the group.
    for (GroupReference const& ref : group->GetMembers())
    {
        Player* dead = ref.GetSource();
        if (!dead || !dead->IsInWorld() || dead->GetMap() != map)
            continue;
        if (dead->IsAlive() || !dead->IsResurrectRequested())
        {
            firstSeen.erase(dead->GetGUID().GetCounter());
            continue;
        }
        if (Services::Lifecycle().is_bot(dead->GetGUID().GetCounter()))
            continue;   // a bot answers its own offers
        bool fromBot = false;
        for (GroupReference const& oref : group->GetMembers())
            if (Player* o = oref.GetSource(); o && o != dead && Services::Lifecycle().is_bot(o->GetGUID().GetCounter()) &&
                dead->IsResurrectRequestedBy(o->GetGUID()))
            { fromBot = true; break; }
        if (!fromBot)
            continue;
        uint32_t& first = firstSeen[dead->GetGUID().GetCounter()];
        if (!first) { first = now_ms ? now_ms : 1u; continue; }
        if (now_ms - first < 3000u)
            continue;
        first = 0;
        TC_LOG_INFO("playerbot.v2.raid", "[raid_rez] {} had a pending resurrection offer from a bot for 3 s without answering: accepted for him",
                    dead->GetName());
        dead->ResurrectUsingRequestData();
    }
}

// Between pulls: brings the raid's tank and healer count up to its quota (RaidRoster.h) from companions that never had a
// role. A role anyone gave is never changed; once a companion takes a role it is stored, so this is a no-op afterwards.
static void FillRaidRoles(uint64_t key, detail::Plan const& plan, uint32_t now_ms)
{
    constexpr uint32_t kFillIntervalMs = 20'000;
    static std::unordered_map<uint64_t, uint32_t> lastFill;
    if (plan.members.size() < 10)
        return;
    uint32_t& last = lastFill[key];
    if (last && now_ms - last < kFillIntervalMs)
        return;
    last = now_ms;

    OwnerRegistry& owners = Services::Owners();
    std::vector<RosterBot> roster;
    std::unordered_map<uint64_t, Player*> byGuid;
    for (detail::Member const& m : plan.members)
    {
        RosterBot b;
        b.guid = m.p->GetGUID().GetCounter();
        b.cls = m.p->GetClass();
        b.role = m.tank ? RosterRole::Tank : m.healer ? RosterRole::Healer : RosterRole::Dps;
        // Humans and bots nobody owns keep what they have.
        b.role_set = !m.bot || owners.GetOwner(b.guid).account_id == 0 || owners.DeclaredRole(b.guid) != Role::Unknown;
        roster.push_back(b);
        byGuid[b.guid] = m.p;
    }
    RosterTargets const targets = RosterTargetsFor(uint32_t(plan.members.size()));
    std::vector<RoleChange> const changes = PlanRaidRoles(roster, targets);
    uint32_t tanks = 0, healers = 0;
    for (RosterBot const& b : roster)
    {
        tanks += b.role == RosterRole::Tank;
        healers += b.role == RosterRole::Healer;
    }
    for (RoleChange const& c : changes)
    {
        Player* p = byGuid[c.guid];
        ::Playerbot::V2::Fleet::BotPopulationManager::ApplyCompanionRole(p, c.role == RosterRole::Healer ? Role::Healer : Role::Tank, "raid_fill");
        TC_LOG_INFO("playerbot.v2.raid", "[raid_roles] {} takes {} for the raid quota (map {} group {})", p->GetName(),
                    c.role == RosterRole::Healer ? "healer" : "tank", plan.mapId, plan.groupLow);
    }
    if (!changes.empty() || healers < targets.healers || tanks < targets.tanks)
        TC_LOG_INFO("playerbot.v2.raid", "[raid_roles] map={} group={} raid {}: healers {}/{} tanks {}/{}, {} role(s) filled{}", plan.mapId,
                    plan.groupLow, plan.members.size(), healers, targets.healers, tanks, targets.tanks, changes.size(),
                    changes.size() ? " (counts above are before the fill)" : ", no unset companion left to fill from");
}

void Brain::Update(uint32_t now_ms)
{
    if (now_ms - last_plan_ms_ < kPlanIntervalMs)
        return;
    last_plan_ms_ = now_ms;

    if (!Services::Config().raid_enabled())
    {
        orders_.clear();
        focus_.clear();
        schools_.clear();
        move_ctx_.clear();
        last_dump_ = "raid brain disabled (Playerbot.Raid.Enable = 0)";
        return;
    }

    LogDeadHumanReleaseState(now_ms);
    EndAbandonedEncounters(now_ms);

    // Bucket bots on raid maps that have rules by (group, map).
    struct Bucket { Group* group = nullptr; uint32_t mapId = 0; uint64_t refBot = 0; Player* ref = nullptr; };
    std::map<uint64_t, Bucket> buckets;
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const& entry)
    {
        if (!entry.ai)
            return;
        Player* p = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(id));
        if (!p || !p->IsInWorld() || p->IsBeingTeleported())
            return;
        Group* group = p->GetGroup();
        Map* map = p->GetMap();
        if (!group || !map || !map->IsDungeon() || !RulesForMap(map->GetId()))
            return;
        uint64_t const key = (group->GetGUID().GetCounter() << 16) ^ uint64_t(map->GetId());
        Bucket& b = buckets[key];
        b.group = group;
        b.mapId = map->GetId();
        if (!b.refBot)
        {
            b.refBot = id;
            b.ref = p;
        }
    });

    std::unordered_map<uint64_t, RaidOrder> next;
    std::unordered_map<uint64_t, ObjectGuid> nextFocus;
    std::unordered_map<uint64_t, std::vector<EnemySchools>> nextSchools;
    std::unordered_map<uint64_t, MoveContext> nextMoveCtx;
    ResetPathProbes();
    std::ostringstream dump;
    for (auto& [key, b] : buckets)
    {
        Plan plan;
        plan.mapId = b.mapId;
        plan.groupLow = b.group->GetGUID().GetCounter();
        plan.rules = RulesForMap(b.mapId);

        // Roles come from the shared group snapshot (it covers humans too).
        std::unordered_map<uint64_t, GroupMemberSummary const*> summaries;
        std::shared_ptr<GroupSnapshot const> gs = Services::Snapshots().latest_group(b.refBot);
        if (gs)
            for (GroupMemberSummary const& s : gs->members)
                summaries[s.guid.GetCounter()] = &s;

        for (GroupReference const& ref : b.group->GetMembers())
        {
            Player* p = ref.GetSource();
            if (!p || !p->IsAlive() || !p->IsInWorld() || p->IsBeingTeleported() || p->GetMap() != b.ref->GetMap())
                continue;
            Member m;
            m.p = p;
            m.bot = Services::Lifecycle().is_bot(p->GetGUID().GetCounter());
            uint8_t const cls = p->GetClass();
            auto it = summaries.find(p->GetGUID().GetCounter());
            uint16_t const spec = it != summaries.end() ? uint16_t(it->second->spec) : 0;
            Role const role = it != summaries.end() ? it->second->role : Role::Unknown;
            m.tank = role == Role::Tank || (role == Role::Unknown && IsTankSpec(cls, spec));
            // A tank-spec bot outside the coordinator's raid tank roster plays DPS (PveOrder::raid_dps_tank).
            if (m.tank && m.bot)
                if (PveOrder const* po = Services::PveCoordinator().OrderFor(p->GetGUID().GetCounter()))
                    if (po->active && po->raid_dps_tank)
                        m.tank = false;
            m.healer = !m.tank && (role == Role::Healer || (role == Role::Unknown && IsHealerSpec(cls, spec)));
            m.ranged = !m.tank && !m.healer && IsRangedSpec(cls, spec);
            m.mana = p->GetMaxPower(POWER_MANA) > 0;
            plan.members.push_back(m);
            if (p->GetHealthPct() < 45.0f)
                plan.emergency = true;
        }
        if (plan.members.size() < 2)
            continue;
        std::sort(plan.members.begin(), plan.members.end(), [](Member const& a, Member const& c) { return a.p->GetGUID() < c.p->GetGUID(); });

        // Main tank: the PvE coordinator's choice when it has one (it ran just before us), else the first tank by guid.
        if (PveOrder const* po = Services::PveCoordinator().OrderFor(b.refBot))
            if (!po->main_tank.IsEmpty())
                for (Member const& m : plan.members)
                    if (m.p->GetGUID() == po->main_tank)
                        plan.mainTank = m.p;
        if (!plan.mainTank)
            for (Member const& m : plan.members)
                if (m.tank) { plan.mainTank = m.p; break; }

        // Enemies: every creature any member is in PvE combat with.
        std::unordered_set<Unit*> seen;
        for (Member const& m : plan.members)
            for (auto const& [guid, ref] : m.p->GetCombatManager().GetPvECombatRefs())
            {
                Unit* other = ref->GetOther(m.p);
                if (!other || other->IsPlayer() || !other->IsAlive() || other->GetMap() != m.p->GetMap())
                    continue;
                if (seen.insert(other).second)
                    plan.enemies.push_back(other);
            }
        std::sort(plan.enemies.begin(), plan.enemies.end(), [](Unit* a, Unit* c) { return a->GetGUID() < c->GetGUID(); });
        TrackPull(key, b.group, plan, b.ref->GetMap(), now_ms);
        SetIdleHostiles(plan, b.ref);
        SetKeepOut(plan);
        SetEngagedReach(plan);
        AcceptBotRezOffers(b.group, b.ref->GetMap(), now_ms);
        LogReleaseState(b.group, b.ref->GetMap(), now_ms);

        // Out of combat only the scripted pre-pull orders (Gothik's side split, Suppression Devices); otherwise the normal
        // follow logic.
        if (plan.enemies.empty())
        {
            FillRaidRoles(key, plan, now_ms);
            WarnBeforePull(plan, b.ref->GetMap(), now_ms);
            MechanicsPrePull(plan, next, dump, now_ms);
            continue;
        }

        dump << plan.rules->name << " group " << b.group->GetGUID().GetCounter() << ": " << plan.members.size() << " members, "
             << plan.enemies.size() << " enemies\n";

        // Change-only plan header, so a log always shows that the raid layer is running and which tanks it uses, even on a
        // pull where no bot gets a move / cast / hold (the per-bot lines below only appear when there is a decision).
        {
            uint32_t tanks = 0, healers = 0;
            for (Member const& m : plan.members)
            {
                if (m.tank) ++tanks;
                if (m.healer) ++healers;
            }
            std::ostringstream header;
            header << plan.rules->name << " group " << b.group->GetGUID().GetCounter() << ": " << plan.members.size() << " alive, "
                   << tanks << " tanks, " << healers << " healers, main tank " << (plan.mainTank ? plan.mainTank->GetName() : std::string("none"));
            static std::unordered_map<uint64_t, std::string> lastHeader;
            std::string const h = header.str();
            if (lastHeader[key] != h)
            {
                lastHeader[key] = h;
                TC_LOG_WARN("playerbot.v2.raid", "[raid] {}", h);
            }
        }

        // Wipe: a human member is dead and the raid is lost (70% of the group dead, or every healer). The survivors stop fighting
        // and retreat to the instance entrance: while a boss still fights anyone, the dead owner cannot release his spirit
        // ("a member of your group is in combat", MC Garr 2026-10-08: two tanks kept fighting after the raid died). The mobs
        // chase or reset; either way combat drops.
        {
            uint32_t total = 0, alive = 0;
            bool humanDead = false;
            for (GroupReference const& gref : b.group->GetMembers())
            {
                Player* gp = gref.GetSource();
                if (!gp || !gp->IsInWorld() || gp->GetMap() != b.ref->GetMap())
                    continue;
                ++total;
                if (gp->IsAlive())
                    ++alive;
                else if (!Services::Lifecycle().is_bot(gp->GetGUID().GetCounter()))
                    humanDead = true;
            }
            bool healersAlive = false;
            for (Member const& m : plan.members)
                healersAlive = healersAlive || m.healer;
            if (total >= 10 && humanDead && (alive * 10 <= total * 3 || !healersAlive))
            {
                auto const* entrance = sObjectMgr->GetMapEntranceTrigger(b.mapId);
                static std::unordered_map<uint64_t, uint32_t> lastWipeLog;
                uint32_t& lw = lastWipeLog[key];
                if (!lw || now_ms - lw >= 10000u)
                {
                    lw = now_ms ? now_ms : 1u;
                    TC_LOG_INFO("playerbot.v2.raid", "[raid_wipe] map={} group={}: {}/{} alive, healers {}, a human member is dead: {} bot(s) retreat to {}",
                                b.mapId, plan.groupLow, alive, total, healersAlive ? "alive" : "all dead", plan.members.size(),
                                entrance ? "the instance entrance" : "nowhere (no entrance known; they stand down in place)");
                }
                if (entrance)
                {
                    for (Member const& m : plan.members)
                        if (m.bot)
                        {
                            RaidOrder& o = next[m.p->GetGUID().GetCounter()];
                            o.active = true;
                            SetMove(o, entrance->Loc.GetPositionX(), entrance->Loc.GetPositionY(), entrance->Loc.GetPositionZ(), "wipe retreat");
                        }
                    continue;
                }
            }
        }

        for (Member const& m : plan.members)
            if (m.bot)
            {
                RaidOrder& o = next[m.p->GetGUID().GetCounter()];
                o.active = true;
                std::vector<Circle> const circles = HazardsFor(plan, m, now_ms);
                nextMoveCtx[m.p->GetGUID().GetCounter()] = BuildMoveContext(plan, m, circles);
                if (!m.p->HasUnitState(UNIT_STATE_ROOT | UNIT_STATE_STUNNED | UNIT_STATE_CONFUSED | UNIT_STATE_FLEEING))
                    DecideMove(plan, m, now_ms, circles, o);
                ApplyKeepOut(plan, m, circles, o);
                if (o.move)
                    dump << "  " << m.p->GetName() << ": move (" << o.move_reason << ")\n";
                for (NoAoe const& rule : plan.rules->noAoe)
                    for (Unit* enemy : plan.enemies)
                        if (enemy->GetEntry() == rule.enemy && m.p->GetDistance(enemy) < 40.0f)
                            o.no_aoe = true;
            }

        // A bot that has to move does not also get a cast job.
        Election e{ plan, next, {}, dump };
        for (Member const& m : plan.members)
            if (m.bot && next[m.p->GetGUID().GetCounter()].move)
                e.busy.insert(m.p->GetGUID().GetCounter());
        std::vector<Unit*> tankTargets;
        for (Member const& m : plan.members)
            if (m.tank)
                if (Unit* v = m.p->GetVictim())
                    tankTargets.push_back(v);
        // A mob a tank is assigned, or that carries a tank marker (star..square), is the tank's, not the mages': the assignment runs a
        // moment ahead of the tank's victim (n32 mc_majordomo: a Healer marked for a tank was sheeped a second later and the tank dropped it).
        {
            Group const* grp = nullptr;
            for (Member const& m : plan.members)
            {
                if (!grp && m.p)
                    grp = m.p->GetGroup();
                if (m.tank && m.bot)
                    if (PveOrder const* po = Services::PveCoordinator().OrderFor(m.p->GetGUID().GetCounter()); po && po->active && !po->tank_target.IsEmpty())
                        for (Unit* enemy : plan.enemies)
                            if (enemy->GetGUID() == po->tank_target)
                                tankTargets.push_back(enemy);
            }
            if (grp)
                // Every marked mob is the owner's call (playtest 2026-10-15): the encounter rules never crowd-control it. The moon
                // is held by the mark orders (PveGroupCoordinator::PlanMarkOrders), not elected a second time here.
                for (uint8 icon = 0; icon < 8; ++icon)
                    if (ObjectGuid const g = grp->GetTargetIcon(icon); !g.IsEmpty())
                        for (Unit* enemy : plan.enemies)
                            if (enemy->GetGUID() == g)
                                tankTargets.push_back(enemy);
        }
        ElectControlledMembers(e);
        MechanicsElect(e, now_ms);   // scripted jobs first (Razuvious' mind control): they must not lose their priest to a dispel
        ElectTankSwaps(e);
        ElectRetaunt(e);
        ElectBerserkerDance(e);
        ElectDispels(e);
        ElectPurges(e);
        ElectCrowdControl(e, tankTargets, CoordinatorFocus(plan.groupLow, plan.mapId));
        PublishTargets(plan, next, CoordinatorFocus(plan.groupLow, plan.mapId));
        if (std::vector<EnemySchools> hints = SchoolHints(plan, dump); !hints.empty())
            nextSchools[FocusKey(plan.groupLow, plan.mapId)] = std::move(hints);
        if (!plan.ruleFocus.IsEmpty())
        {
            nextFocus[FocusKey(plan.groupLow, plan.mapId)] = plan.ruleFocus;
            dump << "  rule kill focus: " << plan.ruleFocus.GetCounter() << "\n";
        }
        PublishThreatHolds(plan, next, now_ms);
        PublishMeleeSoak(plan, next);
        PublishAuraReactions(plan, next);
    }

    // Change-only decision log (WARN so it shows with the usual Logger.playerbot.v2=3 test setup):
    // what each bot was told to do, once per change, for reviewing a wipe in Playerbot.log.
    static std::unordered_map<uint64_t, std::string> lastSignature;
    for (auto const& [guidLow, o] : next)
    {
        std::ostringstream sig;
        if (o.prepull) sig << "pre-pull ";
        if (o.move) sig << "move(" << o.move_reason << ") ";
        if (o.cast_kind != RaidOrder::CastKind::None) sig << "cast " << o.cast_spell << " ";
        if (!o.hold_target.IsEmpty()) sig << "threat-hold ";
        if (o.hold_melee) sig << "melee-hold ";
        if (!o.attack_target.IsEmpty()) sig << "soak ";
        if (o.charm_spell) sig << "charm-cast " << o.charm_spell << " ";
        else if (!o.charm_attack.IsEmpty()) sig << "charm-attack ";
        else if (o.charm_hold) sig << "charm-hold ";
        if (!o.use_object.IsEmpty()) sig << "use-object ";
        if (o.cancel_aura) sig << "cancel-aura " << o.cancel_aura << " ";
        if (o.area_trigger) sig << "area-trigger " << o.area_trigger << " ";
        if (o.hold_rotation) sig << "hold-rotation ";
        std::string const s = sig.str();
        std::string& last = lastSignature[guidLow];
        if (s != last && !s.empty())
            if (Player* p = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(guidLow)))
                TC_LOG_WARN("playerbot.v2.raid", "[raid] {}: {}", p->GetName(), s);
        last = s;
    }
    if (lastSignature.size() > 2 * next.size() + 64)
        lastSignature.clear();

    EndQuietPulls(now_ms);
    orders_.swap(next);
    focus_.swap(nextFocus);
    schools_.swap(nextSchools);
    move_ctx_.swap(nextMoveCtx);
    last_dump_ = dump.str();
    if (!last_dump_.empty())
        TC_LOG_DEBUG("playerbot.v2.raid", "[raid] plan\n{}", last_dump_);

    // Drop per-bot movement memory for bots that are no longer planned.
    for (auto it = g_sticky.begin(); it != g_sticky.end();)
        it = (now_ms - it->second.at > 60000) ? g_sticky.erase(it) : std::next(it);
    if (g_breath.size() > 64)
        g_breath.clear();
}

} // namespace Playerbot::Raid
