// Scripted Blackwing Lair mechanics that the rule tables cannot express. See RaidBrainInternal.h.
//
//  * Razorgore the Untamed, phase 1 (bwl/classic_boss_razorgore.cpp, classic_instance_blackwing_lair.cpp): the raid holds
//    off the adds while somebody takes control of Razorgore through the Orb of Domination (GO 177808, go_orbe_domination:
//    Possess 19832 on Razorgore, Mind Exhaustion 23958 on the user so the next one takes over) and makes him destroy the
//    30 Black Dragon Eggs (GO 177807) with Destroy Egg 19873. Razorgore dying before the last egg kills the raid
//    (MortPhaseUn), so nobody attacks him while eggs are left. The orb user is a DPS bot (never a tank or a healer); a
//    human on the orb is left alone. The adds' threat follows the controller (trigger_orb_of_command), the tanks keep
//    picking them up as usual. Phase 2 (no eggs) is the generic tank-swap rule on Conflagration.
//  * The Suppression Room before Broodlord Lashlayer (classic_instance_blackwing_lair.cpp go_suppression): Suppression
//    Devices (GO 179784) pulse Suppression Aura 22247 every 5 s on everybody near them (the stealthed excepted). A rogue
//    disarms one with Disarm Trap (Spell::EffectOpenLock -> GameObject::Use -> the device's OnGossipHello); it rearms
//    30 s to 2 min later while Broodlord lives. Bot rogues disarm the armed devices near the raid, in the fight and
//    between pulls (then only devices with no unpulled hostile next to them, so a disarm never pulls a pack).

#include "RaidBrainInternal.h"

#include "Creature.h"
#include "GameObject.h"
#include "Player.h"

#include <cmath>
#include <list>

namespace Playerbot::Raid::detail {

namespace {

constexpr uint32_t kRazorgore       = 12435;
constexpr uint32_t kOrbOfDomination = 177808;
constexpr uint32_t kBlackDragonEgg  = 177807;
constexpr uint32_t kMindExhaustion  = 23958;   // on whoever used the orb: no second use for a while
constexpr uint32_t kDestroyEgg      = 19873;   // Razorgore's own spell while possessed
constexpr float    kOrbReach        = 2.5f;

struct OrbUser { uint64_t guid = 0; uint32_t since = 0; };
std::unordered_map<uint32_t, OrbUser> g_orbUser;   // instance id -> the bot walking to the orb

GameObject* Nearest(Unit* from, std::list<GameObject*> const& objects)
{
    GameObject* best = nullptr;
    for (GameObject* go : objects)
        if (go->isSpawned() && (!best || from->GetExactDist2d(go) < from->GetExactDist2d(best)))
            best = go;
    return best;
}

void ElectRazorgoreOrb(Election& e, uint32_t now)
{
    Plan& plan = e.plan;
    Creature* raz = nullptr;
    Member* controller = nullptr;
    for (Member& m : plan.members)
        if (Unit* charmed = m.p->GetCharmed())
            if (charmed->GetEntry() == kRazorgore && charmed->IsAlive())
            {
                raz = charmed->ToCreature();
                controller = &m;
            }
    if (!raz)
        for (Unit* enemy : plan.enemies)
            if (enemy->GetEntry() == kRazorgore)
                raz = enemy->ToCreature();
    if (!raz)
        return;
    // The orb works from the start of the encounter: once the raid is fighting (the guards, or Razorgore himself), a click on the orb
    // makes Razorgore engage the controller (go_orbe_domination). Until then nobody touches him, so waiting for him to be in combat
    // would let a raid that pulls the guards first attack him with the eggs still standing.
    if (!raz->IsInCombat())
    {
        bool fighting = false;
        for (Member const& m : plan.members)
            if (m.p->IsInCombat() && m.p->GetExactDist2d(raz) < 120.f)
                fighting = true;
        if (!fighting)
            return;
    }

    std::list<GameObject*> eggs;
    raz->GetGameObjectListWithEntryInGrid(eggs, kBlackDragonEgg, 250.0f);
    GameObject* egg = Nearest(raz, eggs);
    if (!egg)
    {
        g_orbUser.erase(raz->GetInstanceId());
        return;   // phase 2
    }

    // Phase 1: Razorgore must outlive the eggs.
    for (Member const& m : plan.members)
        if (m.bot)
            if (RaidOrder* o = e.OrderOf(m.p))
                o->forbidden_targets.push_back(raz->GetGUID());

    // A controller drives Razorgore from egg to egg (the API walks him into reach, then casts).
    if (controller)
    {
        g_orbUser.erase(raz->GetInstanceId());
        if (!controller->bot)
            return;
        RaidOrder* o = e.OrderOf(controller->p);
        if (!o)
            return;
        e.busy.insert(controller->p->GetGUID().GetCounter());
        o->move = false;
        o->cast_kind = RaidOrder::CastKind::None;
        o->cast_spell = 0;
        o->charm_spell = kDestroyEgg;
        o->charm_target = egg->GetGUID();
        e.dump << "  " << controller->p->GetName() << ": controls Razorgore, destroying eggs ("
               << raz->GetExactDist2d(egg) << " yd to the next)\n";
        return;
    }

    std::list<GameObject*> orbs;
    raz->GetGameObjectListWithEntryInGrid(orbs, kOrbOfDomination, 200.0f);
    GameObject* orb = Nearest(raz, orbs);
    if (!orb || orb->HasFlag(GO_FLAG_NOT_SELECTABLE))
        return;

    auto eligible = [&](Member const& m)
    {
        return m.bot && !m.tank && !m.healer && !e.busy.count(m.p->GetGUID().GetCounter()) && !m.p->HasAura(kMindExhaustion)
            && !Controlled(m.p) && !m.p->HasUnitState(UNIT_STATE_ROOT | UNIT_STATE_STUNNED | UNIT_STATE_CONFUSED | UNIT_STATE_FLEEING);
    };
    // Keep the same bot walking to the orb (no flapping between two candidates), but not forever.
    OrbUser& sticky = g_orbUser[raz->GetInstanceId()];
    Member* user = nullptr;
    if (sticky.guid && now - sticky.since < 30000)
        for (Member& m : plan.members)
            if (m.p->GetGUID().GetCounter() == sticky.guid && eligible(m))
                user = &m;
    if (!user)
    {
        // Ranged DPS first (melee keep hitting the adds), then the nearest to the orb.
        for (Member& m : plan.members)
        {
            if (!eligible(m))
                continue;
            if (!user || (m.ranged && !user->ranged)
                || (m.ranged == user->ranged && m.p->GetExactDist2d(orb) < user->p->GetExactDist2d(orb)))
                user = &m;
        }
        if (!user)
        {
            e.dump << "  Razorgore: nobody free for the orb\n";
            return;
        }
        sticky = { user->p->GetGUID().GetCounter(), now };
    }

    RaidOrder* o = e.OrderOf(user->p);
    if (!o)
        return;
    e.busy.insert(user->p->GetGUID().GetCounter());
    o->cast_kind = RaidOrder::CastKind::None;
    o->cast_spell = 0;
    if (user->p->GetExactDist2d(orb) > kOrbReach)
        SetMove(*o, orb->GetPositionX(), orb->GetPositionY(), orb->GetPositionZ(), "orb: walk to the Orb of Domination");
    else
    {
        o->move = false;
        o->use_object = orb->GetGUID();
    }
    e.dump << "  " << user->p->GetName() << ": takes the Orb of Domination\n";
}

constexpr uint32_t kSuppressionDevice = 179784;
constexpr uint32_t kDisarmTrap        = 1842;
constexpr float    kDisarmReach       = 3.5f;    // Disarm Trap is a 5-yard spell: stop well inside it
constexpr float    kDeviceRangeFight  = 35.0f;   // in a fight: devices this close to the main tank (or any member)
constexpr float    kDeviceRangeIdle   = 25.0f;   // between pulls: devices this close to the group
constexpr float    kPackClearance     = 15.0f;   // between pulls: no unpulled hostile this close to the device

Ranks const kDisarmTrapRanks = { kDisarmTrap };

struct Disarmer { uint64_t bot = 0; uint32_t since = 0; };
std::unordered_map<uint64_t, Disarmer> g_disarmer;   // device guid low -> the rogue assigned to it

bool Armed(GameObject* device)
{
    return device->isSpawned() && device->GetGoState() == GO_STATE_READY;
}

// An unpulled hostile creature next to the device: walking up to it would start the next pull.
bool PackNear(GameObject* device, Player* rogue)
{
    std::list<Creature*> creatures;
    device->GetCreatureListWithEntryInGrid(creatures, 0, kPackClearance);
    for (Creature* c : creatures)
        if (c->IsAlive() && !c->IsInCombat() && c->IsHostileTo(rogue) && !c->IsCritter())
            return true;
    return false;
}

// One rogue per armed device near the raid. fight: in combat (any device near the fight); else between pulls (devices
// clear of unpulled packs only). Calls back with the rogue and its device.
template <typename Fn>
void PlanDisarms(Plan& plan, std::unordered_set<uint64_t> const& busy, uint32_t now, bool fight, Fn&& give)
{
    Player* anchor = plan.mainTank ? plan.mainTank : plan.members.front().p;
    std::list<GameObject*> found;
    anchor->GetGameObjectListWithEntryInGrid(found, kSuppressionDevice, 60.0f);
    std::vector<GameObject*> devices;
    for (GameObject* device : found)
    {
        if (!Armed(device))
        {
            g_disarmer.erase(device->GetGUID().GetCounter());
            continue;
        }
        bool near = false;
        for (Member const& m : plan.members)
            if (m.p->GetExactDist2d(device) < (fight ? kDeviceRangeFight : kDeviceRangeIdle)
                && std::fabs(m.p->GetPositionZ() - device->GetPositionZ()) < 10.0f)
                near = true;
        if (near)
            devices.push_back(device);
    }
    if (devices.empty())
        return;

    std::unordered_set<uint64_t> taken;
    auto eligible = [&](Member const& m)
    {
        return m.bot && m.p->GetClass() == CLASS_ROGUE && !busy.count(m.p->GetGUID().GetCounter())
            && !taken.count(m.p->GetGUID().GetCounter()) && KnownRank(m.p, kDisarmTrapRanks) && !Controlled(m.p)
            && !m.p->HasUnitState(UNIT_STATE_ROOT | UNIT_STATE_STUNNED | UNIT_STATE_CONFUSED | UNIT_STATE_FLEEING);
    };
    for (GameObject* device : devices)
    {
        Disarmer& sticky = g_disarmer[device->GetGUID().GetCounter()];
        Member* rogue = nullptr;
        if (sticky.bot && now - sticky.since < 30000)
            for (Member& m : plan.members)
                if (m.p->GetGUID().GetCounter() == sticky.bot && eligible(m))
                    rogue = &m;
        if (!rogue)
            for (Member& m : plan.members)
                if (eligible(m) && (!rogue || m.p->GetExactDist2d(device) < rogue->p->GetExactDist2d(device)))
                    rogue = &m;
        if (!rogue)
            return;   // no free rogue left
        if (!fight && PackNear(device, rogue->p))
            continue;
        if (sticky.bot != rogue->p->GetGUID().GetCounter())
            sticky = { rogue->p->GetGUID().GetCounter(), now };
        taken.insert(rogue->p->GetGUID().GetCounter());
        give(*rogue, device);
    }
}

// Walk into reach, then Disarm Trap on the device.
void OrderDisarm(RaidOrder& o, Player* rogue, GameObject* device)
{
    if (rogue->GetExactDist2d(device) > kDisarmReach)
    {
        SetMove(o, device->GetPositionX(), device->GetPositionY(), device->GetPositionZ(), "suppression: walk to the device");
        return;
    }
    o.move = false;
    o.cast_kind = RaidOrder::CastKind::Disarm;
    o.cast_spell = KnownRank(rogue, kDisarmTrapRanks);
    o.cast_target = device->GetGUID();
}

void ElectSuppressionDevices(Election& e, uint32_t now)
{
    PlanDisarms(e.plan, e.busy, now, true, [&](Member& rogue, GameObject* device)
    {
        RaidOrder* o = e.OrderOf(rogue.p);
        if (!o)
            return;
        e.busy.insert(rogue.p->GetGUID().GetCounter());
        OrderDisarm(*o, rogue.p, device);
        e.dump << "  " << rogue.p->GetName() << ": disarms a Suppression Device (" << rogue.p->GetExactDist2d(device) << " yd)\n";
    });
}

} // namespace

void BwlMechanicsElect(Election& e, uint32_t now)
{
    ElectRazorgoreOrb(e, now);
    ElectSuppressionDevices(e, now);
    if (g_orbUser.size() > 64)
        g_orbUser.clear();
    if (g_disarmer.size() > 128)
        g_disarmer.clear();
}

void BwlPrePull(Plan& plan, std::unordered_map<uint64_t, RaidOrder>& orders, std::ostringstream& dump, uint32_t now)
{
    std::unordered_set<uint64_t> const none;
    PlanDisarms(plan, none, now, false, [&](Member& rogue, GameObject* device)
    {
        RaidOrder& o = orders[rogue.p->GetGUID().GetCounter()];
        o.active = true;
        o.prepull = true;
        OrderDisarm(o, rogue.p, device);
        dump << "  " << rogue.p->GetName() << ": disarms a Suppression Device before the pull (" << rogue.p->GetExactDist2d(device)
             << " yd)\n";
    });
}

} // namespace Playerbot::Raid::detail
