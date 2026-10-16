// Scripted Naxxramas mechanics that the rule tables cannot express. See RaidBrainInternal.h.
//
//  * Heigan the Unclean: the eruption dance. The room's four sections erupt in turn
//    (naxx/classic_boss_heigan.cpp UpdateEruption): eruption number k spares section k % 6 and section 6 - k % 6, so the
//    safe section runs 0, 1, 2, 3, 2, 1, 0, ... The counter restarts at the pull, when the dance starts and when it ends.
//    Each eruption summons a short-lived plague wave creature at (2773, -3684, 292); the brain counts those, so the bots
//    always stand in the section the NEXT eruption spares. Sections come from the trap game objects, classified exactly
//    as the instance script does (classic_instance_naxxramas.cpp OnObjectCreate).
//  * Sapphiron: in the air phase the raid spreads (Icebolt splashes and freezes its target into an ice block); when the
//    Frost Breath starts, everybody hides behind the nearest ice block, on the far side from Sapphiron.
//  * Instructor Razuvious: a priest bot mind-controls a Deathknight Understudy, which taunts Razuvious and tanks him
//    (Shield Wall when low). A second priest takes over before the first control runs out. The raid's own tanks leave
//    Razuvious alone while an understudy holds him and keep the free understudies instead; DPS never kill understudies.
//  * Grand Widow Faerlina: after each Enrage a priest mind-controls a Worshipper and sacrifices it (Widow's Embrace).
//  * Gothik the Harvester: he resets unless 10+ living players stand on each side of the combat gate during his speech
//    (naxx/classic_boss_gothik.cpp HasLessPlayersPerSide), and the gate closes at the pull. So before the pull, while a
//    raid member is in his room and the gate is open, the raid splits: tanks, healers and DPS alternate between the live
//    side (y <= gate y, where his waves spawn) and the dead side; humans count on whatever side they stand. In the fight
//    the gate stays closed until he opens it, so every bot only fights enemies on its own side.

#include "RaidBrainInternal.h"

#include "Creature.h"
#include "GameObject.h"
#include "Log.h"
#include "Map.h"
#include "PathGenerator.h"
#include "Player.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellHistory.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

#include <algorithm>
#include <cmath>
#include <list>

namespace Playerbot::Raid::detail {

namespace {

// ---------------------------------------------------------------------------
// Heigan the Unclean
// ---------------------------------------------------------------------------
constexpr uint32_t kHeigan          = 15936;
constexpr uint32_t kHeiganWave      = 17293;    // NPC_HEIGAN_PLAGUE_WAVE
constexpr float    kEruptMarkerX    = 2773.0f;  // UpdateEruption's per-eruption summon
constexpr float    kEruptMarkerY    = -3684.0f;
constexpr float    kInField         = 9.0f;     // closer than this to a trap = standing in the eruption field
constexpr float    kSafeMargin      = 1.5f;     // a spot is safe when the nearest erupting trap is this much farther than the nearest safe one
constexpr uint32_t kNaxxGoSpawnBase = 30000000; // ClassicNaxxGameObjectSpawnId (classic_instance_naxxramas.cpp)
constexpr float    kManaBurnRange   = 28.0f;    // Mana Burn 29310, cast every 3 s while a mana user is this close (fight phase)

struct Trap { float x, y, z; uint8_t section; };

struct HeiganState
{
    bool engaged = false;
    bool dance = false;
    uint32_t eruptions = 0;
    std::vector<ObjectGuid> markers;   // eruption markers already counted
    uint32_t updatedAt = 0;
};

std::unordered_map<uint32_t, std::vector<Trap>> g_heiganTraps;   // instance id -> traps
std::unordered_map<uint32_t, HeiganState> g_heigan;              // instance id -> dance state

// Section of a trap by entry, then the spawn ids the instance script adds explicitly (a trap can be in two sections).
void ClassifyTrap(GameObject* go, std::vector<Trap>& out)
{
    uint32_t const e = go->GetEntry();
    auto add = [&](uint8_t section) { out.push_back({ go->GetPositionX(), go->GetPositionY(), go->GetPositionZ(), section }); };
    if ((e >= 181517 && e <= 181524) || e == 181678)
        add(0);
    else if ((e >= 181510 && e <= 181516) || (e >= 181525 && e <= 181531) || e == 181533 || e == 181676)
        add(1);
    else if ((e >= 181534 && e <= 181544) || e == 181532 || e == 181677)
        add(2);
    else if (e >= 181545 && e <= 181552 && go->GetSpawnId() != kNaxxGoSpawnBase + 533119 && go->GetSpawnId() != kNaxxGoSpawnBase + 533123)
        add(3);
    if (go->GetSpawnId() >= kNaxxGoSpawnBase)
        switch (go->GetSpawnId() - kNaxxGoSpawnBase)
        {
            case 533181: case 533182: case 533183: case 533184: case 533187: case 533188: case 533189: case 533190:
            case 533191: case 533192: case 533193: case 533194: case 533195: case 533197: case 533199: case 533200:
                add(3);
                break;
            case 533185: case 533196: case 533198:
                add(2);
                break;
            default:
                break;
        }
}

std::vector<Trap> const* HeiganTraps(Unit* heigan)
{
    uint32_t const instance = heigan->GetMap()->GetInstanceId();
    auto it = g_heiganTraps.find(instance);
    if (it != g_heiganTraps.end())
        return &it->second;
    std::vector<Trap> traps;
    std::list<GameObject*> objects;
    for (uint32_t entry = 181510; entry <= 181552; ++entry)
        heigan->GetGameObjectListWithEntryInGrid(objects, entry, 120.0f);
    for (uint32_t entry = 181676; entry <= 181678; ++entry)
        heigan->GetGameObjectListWithEntryInGrid(objects, entry, 120.0f);
    for (GameObject* go : objects)
        if (go->GetGoType() == GAMEOBJECT_TYPE_TRAP)
            ClassifyTrap(go, traps);
    // Only cache a complete room (grids around the platform may not be loaded yet); 76 traps in VMaNGOS' data.
    if (traps.size() < 40)
        return nullptr;
    TC_LOG_INFO("playerbot.v2.raid", "[raid] Heigan: {} eruption traps found", traps.size());
    return &(g_heiganTraps[instance] = std::move(traps));
}

void UpdateHeigan(Unit* heigan, uint32_t now)
{
    HeiganState& st = g_heigan[heigan->GetMap()->GetInstanceId()];
    if (st.updatedAt == now)
        return;
    st.updatedAt = now;
    bool const dance = heigan->ToCreature() && heigan->ToCreature()->HasReactState(REACT_PASSIVE);
    if (!st.engaged || dance != st.dance)
    {
        st.engaged = true;
        st.dance = dance;
        st.eruptions = 0;   // the script restarts the eruption counter at the pull and at both phase changes
    }
    std::list<Creature*> waves;
    heigan->GetCreatureListWithEntryInGrid(waves, kHeiganWave, 150.0f);
    for (Creature* wave : waves)
    {
        if (Dist2D(wave->GetPositionX(), wave->GetPositionY(), kEruptMarkerX, kEruptMarkerY) > 2.0f)
            continue;
        if (std::find(st.markers.begin(), st.markers.end(), wave->GetGUID()) != st.markers.end())
            continue;
        st.markers.push_back(wave->GetGUID());
        if (st.markers.size() > 16)
            st.markers.erase(st.markers.begin());
        ++st.eruptions;
    }
}

bool SafeSection(uint32_t eruption, uint8_t section)
{
    uint32_t const k = eruption % 6;
    return section == k || section == 6 - k;
}

// Distance to the nearest trap that erupts next / that does not.
void TrapDistances(std::vector<Trap> const& traps, uint32_t eruption, float x, float y, float& unsafe, float& safe)
{
    unsafe = safe = 9999.0f;
    for (Trap const& t : traps)
    {
        float const d = Dist2D(x, y, t.x, t.y);
        if (SafeSection(eruption, t.section))
            safe = std::min(safe, d);
        else
            unsafe = std::min(unsafe, d);
    }
}

bool HeiganMove(Plan const& plan, Member const& self, uint32_t now, RaidOrder& out)
{
    Unit* heigan = nullptr;
    for (Unit* enemy : plan.enemies)
        if (enemy->GetEntry() == kHeigan)
            heigan = enemy;
    if (!heigan)
    {
        if (!plan.members.empty())
            g_heigan.erase(plan.members.front().p->GetMap()->GetInstanceId());
        return false;
    }
    std::vector<Trap> const* traps = HeiganTraps(heigan);
    if (!traps)
        return false;
    UpdateHeigan(heigan, now);
    HeiganState const& st = g_heigan[heigan->GetMap()->GetInstanceId()];

    Player* bot = self.p;
    float unsafe, safe;
    TrapDistances(*traps, st.eruptions, bot->GetPositionX(), bot->GetPositionY(), unsafe, safe);
    bool const inField = std::min(unsafe, safe) < kInField;
    bool const burnable = self.mana && !self.tank && (self.ranged || self.healer);
    // Fight phase: only whoever stands in the field dances (the ranged on the platform keep casting). Dance phase: everybody.
    if (!st.dance && !inField)
    {
        // Mana users on the platform stay out of Mana Burn range, on a spot outside the eruption field.
        if (!burnable || bot->GetExactDist2d(heigan) >= kManaBurnRange)
            return false;
        float const away = heigan->GetAbsoluteAngle(bot);
        for (float offset : { 0.0f, 0.35f, -0.35f, 0.7f, -0.7f, 1.05f, -1.05f })
        {
            float const x = heigan->GetPositionX() + (kManaBurnRange + 3.0f) * std::cos(away + offset);
            float const y = heigan->GetPositionY() + (kManaBurnRange + 3.0f) * std::sin(away + offset);
            float u, s;
            TrapDistances(*traps, st.eruptions, x, y, u, s);
            float z = 0.0f;
            if (std::min(u, s) >= kInField && Usable(bot, x, y, bot->GetPositionZ(), z, /*urgent*/ true))
                return SetMove(out, x, y, z, "heigan: out of mana burn");
        }
        return false;
    }
    bool const safeHere = st.dance ? (inField && safe + kSafeMargin <= unsafe) : (unsafe >= kInField || safe + kSafeMargin <= unsafe);
    if (safeHere)
    {
        // Melee whose target stands where the next eruption hits wait instead of following it in.
        if (!self.tank && !self.healer && !self.ranged)
            if (Unit* victim = bot->GetVictim())
            {
                float vu, vs;
                TrapDistances(*traps, st.eruptions, victim->GetPositionX(), victim->GetPositionY(), vu, vs);
                if (std::min(vu, vs) < kInField && vs + kSafeMargin > vu)
                {
                    out.hold_melee = true;
                    return true;
                }
            }
        return false;
    }

    // The best spot in the safe section: deep inside it (far from erupting traps), not too far to walk.
    struct Candidate { float score; Trap const* trap; };
    std::vector<Candidate> candidates;
    for (Trap const& t : *traps)
    {
        if (!SafeSection(st.eruptions, t.section))
            continue;
        float u, s;
        TrapDistances(*traps, st.eruptions, t.x, t.y, u, s);
        if (u < kSafeMargin + 1.0f)
            continue;   // a trap on the border of two sections
        float const walk = Dist2D(bot->GetPositionX(), bot->GetPositionY(), t.x, t.y);
        // Mana users prefer spots out of Mana Burn range (fight phase only: in the dance he channels Plague Cloud instead).
        float const burn = (!st.dance && burnable && Dist2D(heigan->GetPositionX(), heigan->GetPositionY(), t.x, t.y) < kManaBurnRange) ? 8.0f : 0.0f;
        candidates.push_back({ std::min(u, 12.0f) - 0.25f * walk - burn, &t });
    }
    std::sort(candidates.begin(), candidates.end(), [](Candidate const& a, Candidate const& b) { return a.score > b.score; });
    int pathChecks = 0;
    for (Candidate const& c : candidates)
    {
        if (++pathChecks > 4)
            break;
        float z = 0.0f;
        if (Usable(bot, c.trap->x, c.trap->y, c.trap->z, z, /*urgent*/ true))
        {
            SetMove(out, c.trap->x, c.trap->y, z, st.dance ? "heigan dance" : "heigan eruption");
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Sapphiron
// ---------------------------------------------------------------------------
constexpr uint32_t kSapphiron        = 15989;
constexpr uint32_t kIceBlock         = 181247;  // GO_SAPPH_ICEBLOCK
constexpr uint32_t kIcebolt          = 28522;   // the frozen player inside a block carries it
constexpr uint32_t kFrostBreath      = 28524;   // 7 s cast from the air
constexpr uint32_t kFrostBreathDummy = 30101;   // the falling ball, 0.5 s before the cast
constexpr float    kIceboltSpread    = 10.0f;

std::unordered_map<uint64_t, uint32_t> g_airSpread;   // bot -> last air phase spread move

bool Casting(Unit* u, uint32_t spell)
{
    if (u->HasAura(spell))
        return true;
    for (CurrentSpellTypes type : { CURRENT_GENERIC_SPELL, CURRENT_CHANNELED_SPELL })
        if (Spell* cast = u->GetCurrentSpell(type))
            if (cast->GetSpellInfo() && cast->GetSpellInfo()->Id == spell)
                return true;
    return false;
}

bool SapphironMove(Plan const& plan, Member const& self, uint32_t now, RaidOrder& out)
{
    Unit* sapphiron = nullptr;
    for (Unit* enemy : plan.enemies)
        if (enemy->GetEntry() == kSapphiron)
            sapphiron = enemy;
    if (!sapphiron || !sapphiron->IsHovering())
        return false;
    Player* bot = self.p;
    if (bot->HasAura(kIcebolt))
        return true;   // frozen inside a block

    if (Casting(sapphiron, kFrostBreath) || Casting(sapphiron, kFrostBreathDummy))
    {
        // Hide: stand right behind the nearest ice block, on the line from Sapphiron through the block.
        std::list<GameObject*> blocks;
        sapphiron->GetGameObjectListWithEntryInGrid(blocks, kIceBlock, 150.0f);
        GameObject* nearest = nullptr;
        float nearestDist = 0.0f;
        for (GameObject* block : blocks)
        {
            if (!block->isSpawned())
                continue;
            float const d = Dist2D(bot->GetPositionX(), bot->GetPositionY(), block->GetPositionX(), block->GetPositionY());
            if (!nearest || d < nearestDist) { nearest = block; nearestDist = d; }
        }
        if (!nearest)
            return false;
        // Melee and tanks stay behind the block instead of chasing Sapphiron (casters may keep casting from cover).
        out.hold_melee = !self.ranged && !self.healer;
        float const away = std::atan2(nearest->GetPositionY() - sapphiron->GetPositionY(), nearest->GetPositionX() - sapphiron->GetPositionX());
        int pathChecks = 0;
        for (float distance : { 3.0f, 2.5f, 4.0f })
            for (float side : { 0.0f, 0.25f, -0.25f })
            {
                float const px = nearest->GetPositionX() + distance * std::cos(away + side);
                float const py = nearest->GetPositionY() + distance * std::sin(away + side);
                if (Dist2D(bot->GetPositionX(), bot->GetPositionY(), px, py) < 1.2f)
                    return true;   // already hidden
                if (++pathChecks > 5)
                    return true;
                float pz = 0.0f;
                if (!Usable(bot, px, py, nearest->GetPositionZ(), pz, /*urgent*/ true))
                    continue;
                SetMove(out, px, py, pz, "hide behind ice block");
                return true;
            }
        return true;
    }

    // Icebolts: keep apart so a splash or a block does not take two players.
    std::vector<Player*> others;
    float nearestRaider = 9999.0f;
    for (Member const& m : plan.members)
        if (m.p != bot)
        {
            others.push_back(m.p);
            nearestRaider = std::min(nearestRaider, Dist2D(bot->GetPositionX(), bot->GetPositionY(), m.p->GetPositionX(), m.p->GetPositionY()));
        }
    if (others.empty() || nearestRaider >= kIceboltSpread * 0.85f)
        return false;
    uint32_t& at = g_airSpread[bot->GetGUID().GetCounter()];
    if (now - at < 3000)
        return false;
    if (Clearest(bot, others, kIceboltSpread, out, "air phase spread", {}, /*urgent*/ true))
    {
        at = now;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Instructor Razuvious
// ---------------------------------------------------------------------------
constexpr uint32_t kRazuvious   = 16061;
constexpr uint32_t kUnderstudy  = 16803;
constexpr uint32_t kUsTaunt     = 29060;   // Deathknight Understudy: Taunt
constexpr uint32_t kUsShieldWall= 29061;   // Deathknight Understudy: Shield Wall (same id the Horsemen use)
Ranks const kMindControl = { 605, 10911, 10912 };

Creature* Possessed(Player* p, uint32_t entry = kUnderstudy)
{
    Unit* charmed = p->GetCharmed();
    if (!charmed || !charmed->IsAlive() || charmed->GetEntry() != entry)
        return nullptr;
    return charmed->ToCreature();
}

bool CreatureSpellReady(Creature* c, uint32_t spell)
{
    SpellInfo const* info = sSpellMgr->GetSpellInfo(spell, DIFFICULTY_NONE);
    return info && c->GetSpellHistory()->IsReady(info);
}

// The Mind Control a priest is casting right now (0 = none).
uint32_t CastingMindControl(Player* p, ObjectGuid& target)
{
    if (Spell* cast = p->GetCurrentSpell(CURRENT_GENERIC_SPELL))
        if (cast->GetSpellInfo() && std::find(kMindControl.begin(), kMindControl.end(), cast->GetSpellInfo()->Id) != kMindControl.end())
        {
            if (ObjectGuid const guid = cast->m_targets.GetUnitTargetGUID(); !guid.IsEmpty())
                target = guid;   // by GUID: the spell's object pointer can dangle between map updates
            return cast->GetSpellInfo()->Id;
        }
    return 0;
}

void ElectRazuvious(Election& e)
{
    Plan& plan = e.plan;
    Unit* raz = nullptr;
    for (Unit* enemy : plan.enemies)
        if (enemy->GetEntry() == kRazuvious)
            raz = enemy;
    if (!raz)
        return;

    std::list<Creature*> understudies;
    raz->GetCreatureListWithEntryInGrid(understudies, kUnderstudy, 80.0f);

    // 1. Controllers command their understudy.
    int holding = 0;            // controllers with more than 12 s of control left
    bool pending = false;       // a Mind Control is being cast
    for (Member& m : plan.members)
    {
        if (!m.bot)
            continue;
        RaidOrder* o = e.OrderOf(m.p);
        if (!o)
            continue;
        ObjectGuid mcTarget;
        if (uint32_t const casting = CastingMindControl(m.p, mcTarget))
        {
            // Keep the cast job so the consumer waits for it instead of letting the rotation cast over it.
            o->move = false;
            o->cast_kind = RaidOrder::CastKind::CrowdControl;
            o->cast_spell = casting;
            o->cast_target = mcTarget;
            e.busy.insert(m.p->GetGUID().GetCounter());
            pending = true;
            continue;
        }
        Creature* us = Possessed(m.p);
        if (!us)
            continue;
        e.busy.insert(m.p->GetGUID().GetCounter());
        o->move = false;   // moving breaks the channel
        o->cast_kind = RaidOrder::CastKind::None;
        o->cast_spell = 0;
        int32_t left = 0;
        for (uint32_t id : kMindControl)
            if (Aura* aura = us->GetAura(id, m.p->GetGUID()))
                left = std::max(left, aura->GetDuration());
        if (left < 0 || left > 12000)   // a negative duration is permanent
            ++holding;
        bool const tanking = raz->GetVictim() == us;
        if (!tanking && us->GetDistance(raz) <= 8.0f && CreatureSpellReady(us, kUsTaunt))
        {
            o->charm_spell = kUsTaunt;
            o->charm_target = raz->GetGUID();
        }
        else if (tanking && us->GetHealthPct() < 60.0f && !us->HasAura(kUsShieldWall) && CreatureSpellReady(us, kUsShieldWall))
            o->charm_spell = kUsShieldWall;
        else
            o->charm_attack = raz->GetGUID();
        e.dump << "  " << m.p->GetName() << ": controls an understudy (" << left / 1000 << " s left)"
               << (tanking ? ", tanking" : "") << "\n";
    }

    // 2. While an understudy holds Razuvious the raid's tanks leave him alone and keep the free understudies.
    bool understudyTanks = false;
    for (Member const& m : plan.members)
        if (Creature* us = Possessed(m.p))
            if (raz->GetVictim() == us)
                understudyTanks = true;
    if (understudyTanks)
        for (Member const& m : plan.members)
        {
            if (!m.bot || !m.tank)
                continue;
            RaidOrder* o = e.OrderOf(m.p);
            if (!o)
                continue;
            o->forbidden_targets.push_back(raz->GetGUID());
            Creature* best = nullptr;
            for (Creature* us : understudies)
                if (us->IsAlive() && !us->IsCharmed() && (!best || m.p->GetDistance(us) < m.p->GetDistance(best)))
                    best = us;
            if (best)
                o->fallback_target = best->GetGUID();
        }

    // 3. A new Mind Control when nobody holds one with time to spare (the next priest takes over before the first runs out).
    if (holding > 0 || pending)
        return;
    Creature* target = nullptr;
    for (Creature* us : understudies)
        if (us->IsAlive() && !us->IsCharmed() && (!target || us->GetDistance(raz) < target->GetDistance(raz)))
            target = us;
    if (target)
        e.Assign(target, { &kMindControl }, true, RaidOrder::CastKind::CrowdControl, "mind control", false);
}

// ---------------------------------------------------------------------------
// Grand Widow Faerlina
// ---------------------------------------------------------------------------
constexpr uint32_t kFaerlina      = 15953;
constexpr uint32_t kWorshipper    = 16506;
constexpr uint32_t kFaerlinaRage  = 28798;   // Enrage, every 60 s
constexpr uint32_t kWidowsEmbrace = 28732;   // removes the enrage and blocks the next one and her volley for 30 s

// Faerlina: once she enrages, a priest mind-controls a Naxxramas Worshipper and makes it cast Widow's Embrace on her
// (naxx/classic_boss_faerlina.cpp SpellHit; the worshipper has no bot logic of its own, the spell is its template spell).
// Sacrificing after the enrage, not before, buys the full minute.
void ElectFaerlina(Election& e)
{
    Plan& plan = e.plan;
    Unit* faerlina = nullptr;
    for (Unit* enemy : plan.enemies)
        if (enemy->GetEntry() == kFaerlina)
            faerlina = enemy;
    if (!faerlina)
        return;
    bool const enraged = faerlina->HasAura(kFaerlinaRage) && !faerlina->HasAura(kWidowsEmbrace);

    bool controlling = false;
    for (Member& m : plan.members)
    {
        if (!m.bot)
            continue;
        RaidOrder* o = e.OrderOf(m.p);
        if (!o)
            continue;
        ObjectGuid mcTarget;
        if (uint32_t const casting = CastingMindControl(m.p, mcTarget))
        {
            o->move = false;
            o->cast_kind = RaidOrder::CastKind::CrowdControl;
            o->cast_spell = casting;
            o->cast_target = mcTarget;
            e.busy.insert(m.p->GetGUID().GetCounter());
            controlling = true;
            continue;
        }
        Creature* worshipper = Possessed(m.p, kWorshipper);
        if (!worshipper)
            continue;
        controlling = true;
        e.busy.insert(m.p->GetGUID().GetCounter());
        o->move = false;   // moving breaks the channel
        o->cast_kind = RaidOrder::CastKind::None;
        o->cast_spell = 0;
        if (!enraged)
            o->charm_hold = true;
        else if (worshipper->GetDistance(faerlina) > 8.0f)
            o->charm_attack = faerlina->GetGUID();   // walk it over
        else
        {
            o->charm_spell = kWidowsEmbrace;
            o->charm_target = faerlina->GetGUID();
        }
        e.dump << "  " << m.p->GetName() << ": controls a worshipper" << (enraged ? ", sacrificing it" : "") << "\n";
    }
    if (controlling || !enraged)
        return;

    std::list<Creature*> worshippers;
    faerlina->GetCreatureListWithEntryInGrid(worshippers, kWorshipper, 60.0f);
    Creature* target = nullptr;
    for (Creature* w : worshippers)
        if (w->IsAlive() && !w->IsCharmed() && (!target || w->GetDistance(faerlina) < target->GetDistance(faerlina)))
            target = w;
    if (target)
        e.Assign(target, { &kMindControl }, true, RaidOrder::CastKind::CrowdControl, "mind control", false);
}

// ---------------------------------------------------------------------------
// Gothik the Harvester
// ---------------------------------------------------------------------------
constexpr uint32_t kGothik      = 16060;
constexpr uint32_t kCombatGate  = 181170;   // GO_MILI_GOTH_COMBAT_GATE, spawned at (2692.07, -3360.78, 267.5)
constexpr float    kGothRoom    = 60.0f;    // a member this close to the gate is in his room
constexpr uint32_t kSideMinimum = 10;       // HasLessPlayersPerSide(10)
constexpr uint32_t kGothImmune  = 29230;    // on the balcony (speech and waves) he takes no damage

GameObject* GothikGate(Player* anyone)
{
    std::list<GameObject*> gates;
    anyone->GetGameObjectListWithEntryInGrid(gates, kCombatGate, 120.0f);
    return gates.empty() ? nullptr : gates.front();
}

bool LiveSide(GameObject* gate, float y) { return y <= gate->GetPositionY(); }   // IsInRightSideGothArea

struct GothSpot { uint32_t instance = 0; bool live = false; float x = 0, y = 0, z = 0; };
std::unordered_map<uint64_t, GothSpot> g_gothSpot;                  // bot -> its pre-pull spot
std::unordered_map<uint64_t, std::pair<bool, Position>> g_gothAnchor; // (instance << 1 | side) -> anchor found?

// A floor point on one side, reachable on foot (the gate is open before the pull, and the navmesh ignores it anyway).
bool GothAnchor(Player* bot, GameObject* gate, bool live, Position& out)
{
    uint64_t const key = (uint64_t(bot->GetInstanceId()) << 1) | (live ? 1 : 0);
    auto it = g_gothAnchor.find(key);
    if (it != g_gothAnchor.end())
    {
        out = it->second.second;
        return it->second.first;
    }
    float const sign = live ? -1.0f : 1.0f;
    for (float depth : { 18.0f, 14.0f, 24.0f, 10.0f })
        for (float along : { 0.0f, 8.0f, -8.0f, 16.0f, -16.0f })
        {
            float const x = gate->GetPositionX() + along;
            float const y = gate->GetPositionY() + sign * depth;
            float const z = bot->GetMap()->GetHeight(bot->GetPhaseShift(), x, y, gate->GetPositionZ() + 2.0f, true, 8.0f);
            if (z <= INVALID_HEIGHT + 1.0f || std::fabs(z - gate->GetPositionZ()) > 4.0f)
                continue;
            PathGenerator path(bot);
            if (!path.CalculatePath(x, y, z, false) || (path.GetPathType() & (PATHFIND_NOPATH | PATHFIND_INCOMPLETE)))
                continue;
            out.Relocate(x, y, z);
            g_gothAnchor[key] = { true, out };
            return true;
        }
    g_gothAnchor[key] = { false, Position() };
    TC_LOG_WARN("playerbot.v2.raid", "[raid] Gothik: no reachable {} side spot near the combat gate", live ? "live" : "dead");
    return false;
}

void NaxxPrePullGothik(Plan& plan, std::unordered_map<uint64_t, RaidOrder>& orders, std::ostringstream& dump)
{
    Player* first = plan.members.front().p;
    GameObject* gate = GothikGate(first);
    if (!gate)
        return;
    Creature* gothik = gate->FindNearestCreature(kGothik, 120.0f, true);
    if (!gothik || gothik->IsInCombat() || gate->GetGoState() != GO_STATE_ACTIVE)
        return;
    bool inRoom = false;
    for (Member const& m : plan.members)
        if (m.p->GetExactDist2d(gate) < kGothRoom && std::fabs(m.p->GetPositionZ() - gate->GetPositionZ()) < 8.0f)
            inRoom = true;
    if (!inRoom || plan.members.size() < 2 * kSideMinimum)
        return;

    // Humans stay where they are and count for that side; bots alternate within each role so both sides get tanks,
    // healers and damage, starting with the side that has fewer humans.
    uint32_t live = 0, dead = 0;
    for (Member const& m : plan.members)
        if (!m.bot)
            ++(LiveSide(gate, m.p->GetPositionY()) ? live : dead);
    bool nextLive = live <= dead;
    std::vector<Member const*> ordered;
    for (int pass = 0; pass < 3; ++pass)
        for (Member const& m : plan.members)
            if (m.bot && (pass == 0 ? m.tank : pass == 1 ? m.healer : (!m.tank && !m.healer)))
                ordered.push_back(&m);
    uint32_t const instance = first->GetInstanceId();
    uint32_t index = 0;
    for (Member const* m : ordered)
    {
        bool const side = nextLive;
        nextLive = !nextLive;
        ++index;
        GothSpot& spot = g_gothSpot[m->p->GetGUID().GetCounter()];
        if (spot.instance != instance || spot.live != side)
        {
            Position anchor;
            if (!GothAnchor(m->p, gate, side, anchor))
                continue;
            // A small ring around the anchor, a different place per bot, on the floor.
            float const angle = float(index) * 2.4f;
            float const radius = 2.0f + float(index % 4) * 1.5f;
            float x = anchor.GetPositionX() + radius * std::cos(angle);
            float y = anchor.GetPositionY() + radius * std::sin(angle);
            float z = m->p->GetMap()->GetHeight(m->p->GetPhaseShift(), x, y, anchor.GetPositionZ() + 2.0f, true, 6.0f);
            if (z <= INVALID_HEIGHT + 1.0f || std::fabs(z - anchor.GetPositionZ()) > 3.0f || LiveSide(gate, y) != side)
            {
                x = anchor.GetPositionX();
                y = anchor.GetPositionY();
                z = anchor.GetPositionZ();
            }
            spot = { instance, side, x, y, z };
        }
        ++(side ? live : dead);
        RaidOrder& o = orders[m->p->GetGUID().GetCounter()];
        o.active = true;
        o.prepull = true;
        SetMove(o, spot.x, spot.y, spot.z, side ? "gothik: live side" : "gothik: dead side");
    }
    dump << "  Gothik split: " << live << " live side, " << dead << " dead side\n";
}

// In the fight the gate is closed: a bot only fights enemies on its own side (the other side is out of reach).
void ElectGothikSides(Election& e)
{
    Plan& plan = e.plan;
    bool gothik = false;
    for (Unit* enemy : plan.enemies)
        if (enemy->GetEntry() == kGothik)
            gothik = true;
    if (!gothik)
        return;
    GameObject* gate = GothikGate(plan.members.front().p);
    if (!gate || gate->GetGoState() == GO_STATE_ACTIVE)   // the gate is open: one room again
        return;
    for (Member const& m : plan.members)
    {
        if (!m.bot)
            continue;
        RaidOrder* o = e.OrderOf(m.p);
        if (!o)
            continue;
        bool const side = LiveSide(gate, m.p->GetPositionY());
        Unit* nearest = nullptr;
        for (Unit* enemy : plan.enemies)
        {
            if (LiveSide(gate, enemy->GetPositionY()) != side || enemy->HasAura(kGothImmune))
                o->forbidden_targets.push_back(enemy->GetGUID());
            else if (enemy->GetEntry() != kGothik && (!nearest || m.p->GetDistance(enemy) < m.p->GetDistance(nearest)))
                nearest = enemy;
        }
        if (nearest && o->fallback_target.IsEmpty())
            o->fallback_target = nearest->GetGUID();
    }
}

} // namespace

void NaxxPrePull(Plan& plan, std::unordered_map<uint64_t, RaidOrder>& orders, std::ostringstream& dump)
{
    NaxxPrePullGothik(plan, orders, dump);
    if (g_gothSpot.size() > 512)
        g_gothSpot.clear();
    if (g_gothAnchor.size() > 64)
        g_gothAnchor.clear();
}

bool NaxxMechanicsMove(Plan const& plan, Member const& self, uint32_t now, RaidOrder& out)
{
    return HeiganMove(plan, self, now, out) || SapphironMove(plan, self, now, out);
}

void NaxxMechanicsElect(Election& e, uint32_t /*now*/)
{
    ElectRazuvious(e);
    ElectFaerlina(e);
    ElectGothikSides(e);
    if (g_airSpread.size() > 256)
        g_airSpread.clear();
}

} // namespace Playerbot::Raid::detail
