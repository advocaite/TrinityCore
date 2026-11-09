// A boss fight script's brain pass. See RaidFightScript.h.
//
// Runs after the generic rule passes of a plan. For the DPS bots of a scripted boss it replaces the threat hold with a fixed start wait,
// names one kill target for everybody, and drops any crowd-control job aimed at the fight's own enemies. Tanks and healers are not touched:
// their assignment, pick-up and healing rules are the generic ones.

#include "RaidBrainInternal.h"
#include "RaidFightScript.h"

#include "Creature.h"
#include "Group.h"
#include "Log.h"
#include "Player.h"

#include <algorithm>
#include <unordered_map>

namespace Playerbot::Raid::detail {

namespace {

struct FightState
{
    uint32_t since = 0;    // first plan pass with an engaged enemy of the fight
    uint32_t last = 0;     // last plan pass with one
    ObjectGuid focus;      // last pick (hysteresis, change-only log)
    uint32_t startLogged = 0;
};
std::unordered_map<uint64_t, FightState> g_fights;   // (group, map)
std::unordered_map<uint64_t, uint64_t> g_ownPick;     // bot -> its last pick when it could not take the raid's focus (Fight::schoolAware)

constexpr uint32_t kFightGapMs = 8000;   // no engaged enemy for this long: the next pull starts a new fight
constexpr uint8_t kSkullIcon = 7, kCrossIcon = 6;

} // namespace

void ApplyFightScript(Plan& plan, std::unordered_map<uint64_t, RaidOrder>& orders, uint32_t now, std::ostringstream& dump)
{
    Script::Fight const* fight = nullptr;
    for (Unit* enemy : plan.enemies)
        if ((fight = Script::FightFor(plan.mapId, enemy->GetEntry())))
            break;
    if (!fight)
        return;

    // Only enemies the fight names count (a stray pack mob near the room does not start the clock or become the focus).
    std::vector<Unit*> mine;
    for (Unit* enemy : plan.enemies)
        if (Script::FightFor(plan.mapId, enemy->GetEntry()) == fight && enemy->GetVictim())
            mine.push_back(enemy);
    if (mine.empty())
        return;

    uint64_t const key = (plan.groupLow << 16) ^ uint64_t(plan.mapId);
    FightState& st = g_fights[key];
    if (!st.since || now - st.last > kFightGapMs)
    {
        st.since = now;
        st.focus = ObjectGuid::Empty;
        st.startLogged = 0;
    }
    st.last = now;
    bool const wait = Script::StartWait(st.since, now, fight->startWaitMs);
    // A timed kill the fight needs now, whatever the kill order says (Zul'Gurub: a Son of Hakkar before Blood Siphon).
    Unit* const forced = plan.mapId == 309 ? ZgForcedFocus(plan, now) : nullptr;

    // The owner's skull, else cross.
    Group const* grp = nullptr;
    for (Member const& m : plan.members)
        if (m.p && (grp = m.p->GetGroup()))
            break;
    ObjectGuid ownerMark;
    if (grp)
        for (uint8_t icon : { kSkullIcon, kCrossIcon })
        {
            if (fight->skullOnly && icon != kSkullIcon)
                continue;
            ObjectGuid const g = grp->GetTargetIcon(icon);
            if (g.IsEmpty())
                continue;
            for (Unit* enemy : mine)
                if (enemy->GetGUID() == g)
                    ownerMark = g;
            if (!ownerMark.IsEmpty())
                break;
        }

    // Fight::heldFirst: the enemies a tank of the raid holds.
    std::vector<Unit*> held;
    if (fight->heldFirst)
        for (Unit* enemy : mine)
            if (Unit* v = enemy->GetVictim())
                if (Player* vp = v->ToPlayer())
                    if (Member const* vm = FindMember(plan, vp); vm && vm->tank)
                        held.push_back(enemy);

    // Fight::schoolAware: the schools each enemy of the fight avoids (EnemySchoolMasks, as the generic target pick reads them).
    std::vector<uint8_t> avoid(mine.size(), 0);
    if (fight->schoolAware)
    {
        Unit const* const schoolCaster = SchoolReferenceCaster(plan);
        for (size_t i = 0; i < mine.size(); ++i)
        {
            if (Creature const* c = mine[i]->ToCreature(); c && c->IsInEvadeMode())
                continue;
            uint8_t prefer = 0;
            EnemySchoolMasks(mine[i], schoolCaster, prefer, avoid[i]);
        }
    }

    ObjectGuid chosen;   // the raid's focus this pass: the pick most DPS bots were given
    std::unordered_map<uint64_t, size_t> votes;
    size_t dps = 0;
    for (Member const& m : plan.members)
    {
        if (!m.bot)
            continue;
        auto it = orders.find(m.p->GetGUID().GetCounter());
        if (it == orders.end())
            continue;
        RaidOrder& o = it->second;

        // A crowd-control job on the fight's own enemies: nobody sheeps a Protector (no source says it works), and the mark orders are the
        // owner's. Everything else the election chose stays.
        if (o.cast_kind == RaidOrder::CastKind::CrowdControl)
            for (Unit* enemy : mine)
                if (enemy->GetGUID() == o.cast_target)
                {
                    o.cast_kind = RaidOrder::CastKind::None;
                    o.cast_spell = 0;
                    o.cast_target = ObjectGuid::Empty;
                    break;
                }

        if (m.tank || m.healer)
            continue;
        // Fight::freeAura: this bot has its own enemy the others cannot see (a Shade of Jin'do); the generic rules keep it.
        if (fight->freeAura && m.p->HasAura(fight->freeAura))
            continue;
        if (forced)
        {
            o.script_owns = true;
            o.script_focus = forced->GetGUID();
            o.fallback_target = forced->GetGUID();
            o.hold_target = ObjectGuid::Empty;
            ++votes[forced->GetGUID().GetCounter()];
            ++dps;
            continue;
        }

        // Skull only: everything else of the fight, marked or sheeped or not, is off limits for this bot.
        if (fight->skullOnly)
            for (Unit* enemy : mine)
                if (enemy->GetGUID() != ownerMark)
                    o.forbidden_targets.push_back(enemy->GetGUID());

        uint8_t const schools = fight->schoolAware ? MemberDamageSchools(m, plan.mainTank) : uint8_t(0);
        std::vector<Script::Enemy> view;
        view.reserve(mine.size());
        for (size_t i = 0; i < mine.size(); ++i)
        {
            Unit* enemy = mine[i];
            Script::Enemy e;
            e.guid = enemy->GetGUID().GetCounter();
            e.entry = enemy->GetEntry();
            e.hpPct = enemy->GetHealthPct();
            bool const forbidden = std::find(o.forbidden_targets.begin(), o.forbidden_targets.end(), enemy->GetGUID()) != o.forbidden_targets.end();
            Creature const* c = enemy->ToCreature();
            // Not one that cannot be hit now (Arlokk vanished: invisible, uninteractible, not attackable).
            e.eligible = enemy->IsAlive() && !forbidden && !(c && c->IsInEvadeMode()) && enemy->IsVisible() && !enemy->IsUninteractible() &&
                         !enemy->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
            if (fight->schoolAware && schools && (schools & ~avoid[i]) == 0)
                e.eligible = false;
            e.held = std::find(held.begin(), held.end(), enemy) != held.end();
            view.push_back(e);
        }
        // The raid's focus is this bot's previous pick while it can take it; a bot that cannot keeps its own pick until that one dies.
        uint64_t previous = st.focus.IsEmpty() ? 0 : st.focus.GetCounter();
        uint64_t const self = m.p->GetGUID().GetCounter();
        if (fight->schoolAware && previous)
        {
            bool canTake = false;
            for (Script::Enemy const& e : view)
                if (e.guid == previous && e.eligible)
                    canTake = true;
            if (!canTake)
                if (auto own = g_ownPick.find(self); own != g_ownPick.end())
                    previous = own->second;
        }
        uint64_t const pick = Script::PickFocus(*fight, view.data(), view.size(), ownerMark.IsEmpty() ? 0 : ownerMark.GetCounter(), previous);
        if (fight->schoolAware)
        {
            if (pick && st.focus.GetCounter() != pick)
                g_ownPick[self] = pick;
            else
                g_ownPick.erase(self);
        }
        ObjectGuid focus;
        for (Unit* enemy : mine)
            if (pick && enemy->GetGUID().GetCounter() == pick)
                focus = enemy->GetGUID();
        if (focus.IsEmpty())
        {
            // The skull carries a shield this bot must not strike (Damage Shield for melee, Magic Reflection for casters): it stands, the
            // generic pickers would start an attack on the skull each tick and every swing is reflected.
            if (fight->skullOnly && !ownerMark.IsEmpty() &&
                std::find(o.forbidden_targets.begin(), o.forbidden_targets.end(), ownerMark) != o.forbidden_targets.end())
            {
                o.script_owns = true;
                o.script_focus = ownerMark;
                o.hold_target = ownerMark;
            }
            continue;   // nothing the script can name for this bot: the generic rules keep it
        }

        o.script_owns = true;
        o.script_focus = focus;
        o.fallback_target = focus;
        // The one hold of a scripted fight: the start wait. No threat ratio, no over-threat release clocks.
        o.hold_target = wait ? focus : ObjectGuid::Empty;
        ++votes[focus.GetCounter()];
        ++dps;
    }
    {
        size_t most = 0;
        uint64_t top = 0;
        for (auto const& [g, n] : votes)
            if (n > most || (n == most && g < top))
            {
                most = n;
                top = g;
            }
        for (Unit* enemy : mine)
            if (top && enemy->GetGUID().GetCounter() == top)
                chosen = enemy->GetGUID();
        if (forced && top == forced->GetGUID().GetCounter())
            chosen = forced->GetGUID();
    }

    if (chosen != st.focus)
    {
        st.focus = chosen;
        std::string name = "-";
        for (Unit* enemy : mine)
            if (enemy->GetGUID() == chosen)
                name = enemy->GetName();
        if (forced && forced->GetGUID() == chosen)
            name = forced->GetName() + " (timed kill)";
        TC_LOG_INFO("playerbot.v2", "[fight_script] group={} map={} {}: DPS focus -> {} ({}) owner mark {} dps bots {}", plan.groupLow, plan.mapId,
            fight->name, name, chosen.GetCounter(), ownerMark.IsEmpty() ? "none" : "set", dps);
    }
    if (wait && !st.startLogged)
    {
        st.startLogged = now;
        TC_LOG_INFO("playerbot.v2", "[fight_script] group={} map={} {}: fight starts, DPS wait {} ms", plan.groupLow, plan.mapId, fight->name,
            fight->startWaitMs);
    }
    dump << "  fight script " << fight->name << ": focus " << chosen.GetCounter() << (wait ? " (start wait)" : "") << "\n";
}

} // namespace Playerbot::Raid::detail
