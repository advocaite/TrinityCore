// Scripted Zul'Gurub mechanics that the rule tables cannot express. See RaidBrainInternal.h and ZgHakkar.h.
//
//  * Hakkar's Blood Siphon: 25 s before each siphon the DPS kill one Son of Hakkar (ZgForcedFocus, taken by the fight script), and once it
//    is low every raider but Hakkar's tank and a Corrupted Blood carrier stands next to it (ZgMechanicsMove), so its death gives them
//    Poisonous Blood. One [raid_siphon] line opens each kill window (Sons in the fight, idle, dead; who is poisoned) and one says how many
//    carried the poison when the siphon came, so a run shows whether it worked.
//  * The Sons stand idle round the room until somebody hits one. When a kill window opens with no Son in the fight, the off-tank (a bot
//    tank Hakkar is not on) walks to the idle Son nearest to Hakkar and takes it (RaidOrder::attack_target), so the Son is held next to
//    the raid while the DPS kill it. Before, a ranged bot pulled it with one shot and the Son ran at the shooter: the hunter fed it to
//    Feign Death (LooseMobEscape), the Son left the fight, no Son died before a siphon (sweep-zg-1109y: 0 of 19 and 0 of 20 raiders
//    poisoned at every siphon) and the loose Son killed a healer, a hunter and a rogue. Only with no off-tank alive does a ranged bot (a
//    hunter first, then a mage, then a warlock) still pull with one shot (ZgMechanicsElect), walking into reach first (ZgMechanicsMove).

#include "RaidBrainInternal.h"
#include "ZgHakkar.h"

#include "Creature.h"
#include "SharedDefines.h"
#include "Log.h"
#include "Player.h"

#include <iterator>
#include <list>
#include <unordered_map>

namespace Playerbot::Raid::detail {

namespace {

using namespace Playerbot::Raid::Zg;

struct SiphonClock
{
    uint64_t group = 0;
    uint32_t start = 0;        // first plan pass with Hakkar in combat
    uint32_t lastSiphon = 0;   // ms into the fight the siphon was last seen (0: none yet)
    uint32_t seenAt = 0;       // plan pass that saw it (one siphon is many passes)
    uint32_t windowLogged = 0; // the due time whose kill window was logged
    uint32_t pass = 0;         // the plan pass the clock was last brought up to date in
    uint32_t poisoned = 0, raiders = 0;   // this pass
    ObjectGuid son;            // the Son the DPS are on (kept until it dies or the window closes)
    ObjectGuid pullSon;        // the idle Son being pulled (no Son in the fight yet)
    ObjectGuid puller;         // the bot that pulls it
    bool pullerIsTank = false; // the puller is the off-tank: it walks in and takes the Son itself, no ranged walk
};

// The ranged openers of a pull: one shot from range makes the Son run at the shooter.
Ranks const kArcaneShot = { 3044, 14281, 14282, 14283, 14284, 14285, 14286, 14287 };
Ranks const kFrostbolt  = { 116, 205, 837, 7322, 8406, 8407, 8408, 10179, 10180, 10181, 25304 };
Ranks const kShadowBolt = { 686, 695, 705, 1088, 1106, 7641, 11659, 11660, 11661, 25307 };
constexpr float kPullReach = 28.0f;   // the puller walks to this far from the Son (inside the 30-35 y of the openers)
std::unordered_map<uint64_t, SiphonClock> g_siphon;   // by Hakkar's guid

Unit* FindHakkar(Plan const& plan)
{
    for (Unit* u : plan.enemies)
        if (u->GetEntry() == kHakkar && u->IsAlive() && u->IsInCombat())
            return u;
    return nullptr;
}

bool Siphoning(Unit* hakkar)
{
    return hakkar->HasAura(kBloodSiphon) || hakkar->HasAura(kSiphonFeed) || hakkar->HasAura(kSiphonPoison);
}

bool HeldByTank(Plan const& plan, Unit* u)
{
    if (Unit* v = u->GetVictim())
        if (Player* vp = v->ToPlayer())
            for (Member const& m : plan.members)
                if (m.p == vp && m.tank)
                    return true;
    return false;
}

// Brings Hakkar's clock up to date once per plan pass; nullptr outside his fight. t: ms into the fight.
SiphonClock* Clock(Plan const& plan, uint32_t now, Unit*& hakkar, uint32_t& t)
{
    hakkar = FindHakkar(plan);
    if (!hakkar)
    {
        // Out of his fight (a wipe, a kill): forget this raid's clock, the next pull starts again at 90 s.
        for (auto it = g_siphon.begin(); it != g_siphon.end();)
            it = it->second.group == plan.groupLow ? g_siphon.erase(it) : std::next(it);
        return nullptr;
    }
    SiphonClock& c = g_siphon[hakkar->GetGUID().GetCounter()];
    if (!c.start)
    {
        c.start = now ? now : 1u;
        c.group = plan.groupLow;
    }
    t = now - c.start;
    if (c.pass == now)
        return &c;
    c.pass = now;

    c.poisoned = 0;
    c.raiders = 0;
    for (Member const& m : plan.members)
    {
        ++c.raiders;
        if (m.p->HasAura(kPoisonousBlood))
            ++c.poisoned;
    }
    if (Siphoning(hakkar) && (!c.seenAt || now - c.seenAt > 30000u))
    {
        c.seenAt = now;
        c.lastSiphon = t ? t : 1u;
        c.son = ObjectGuid::Empty;
        TC_LOG_INFO("playerbot.v2.raid", "[raid_siphon] group={} Hakkar's Blood Siphon at {} s: {} of {} raiders carried Poisonous Blood; next due at {} s",
            plan.groupLow, t / 1000u, c.poisoned, c.raiders, SiphonDueMs(c.lastSiphon) / 1000u);
    }
    uint32_t const due = SiphonDueMs(c.lastSiphon);
    if (t + kSiphonLeadMs >= due && c.windowLogged != due)
    {
        c.windowLogged = due;
        std::list<Creature*> sons;
        hakkar->GetCreatureListWithEntryInGrid(sons, kSonOfHakkar, 100.0f);
        size_t idle = 0, engaged = 0, dead = 0;
        for (Creature* son : sons)
        {
            if (!son->IsAlive())
                ++dead;
            else if (son->IsInCombat())
                ++engaged;
            else
                ++idle;
        }
        TC_LOG_INFO("playerbot.v2.raid", "[raid_siphon] group={} Hakkar at {} s, siphon due at {} s: Sons of Hakkar within 100 y {} in the fight, {} idle, {} dead; "
            "Poisonous Blood on {} of {}", plan.groupLow, t / 1000u, due / 1000u, engaged, idle, dead, c.poisoned, c.raiders);
    }
    return &c;
}

// The Son the raid kills now, if any (kept while it lives, so the crowd does not switch Sons).
Unit* SonToKill(Plan const& plan, SiphonClock& c, uint32_t t)
{
    if (!KillWindow(t, c.lastSiphon, c.poisoned, c.raiders))
    {
        c.son = ObjectGuid::Empty;
        return nullptr;
    }
    Unit* best = nullptr;
    bool bestHeld = false;
    for (Unit* u : plan.enemies)
    {
        if (u->GetEntry() != kSonOfHakkar || !u->IsAlive())
            continue;
        if (u->GetGUID() == c.son)
            return u;
        // One a tank holds first (the healers are not chased across the room), then the most hurt.
        bool const held = HeldByTank(plan, u);
        if (!best || (held && !bestHeld) || (held == bestHeld && u->GetHealthPct() < best->GetHealthPct()))
        {
            best = u;
            bestHeld = held;
        }
    }
    if (best && best->GetGUID() != c.son)
    {
        c.son = best->GetGUID();
        TC_LOG_INFO("playerbot.v2.raid", "[raid_siphon] group={} the DPS kill a Son of Hakkar ({}, {:.0f}%, {}) {} s before the siphon", plan.groupLow,
            best->GetGUID().GetCounter(), best->GetHealthPct(), bestHeld ? "held by a tank" : "loose", (SiphonDueMs(c.lastSiphon) - t) / 1000u);
    }
    return best;
}

// The idle Son the raid pulls now: inside a kill window with no Son in the fight, the one nearest to Hakkar (kept while it stays idle).
Creature* SonToPull(Plan const& plan, SiphonClock& c, Unit* hakkar, uint32_t t)
{
    bool engaged = false;
    for (Unit* u : plan.enemies)
        if (u->GetEntry() == kSonOfHakkar && u->IsAlive())
            engaged = true;
    if (engaged || !KillWindow(t, c.lastSiphon, c.poisoned, c.raiders))
    {
        c.pullSon = ObjectGuid::Empty;
        c.puller = ObjectGuid::Empty;
        c.pullerIsTank = false;
        return nullptr;
    }
    std::list<Creature*> sons;
    hakkar->GetCreatureListWithEntryInGrid(sons, kSonOfHakkar, 100.0f);
    Creature* best = nullptr;
    for (Creature* son : sons)
    {
        if (!son->IsAlive() || son->IsInCombat() || son->IsInEvadeMode())
            continue;
        if (son->GetGUID() == c.pullSon)
            return son;
        if (!best || hakkar->GetExactDist2d(son) < hakkar->GetExactDist2d(best))
            best = son;
    }
    if (best)
    {
        c.pullSon = best->GetGUID();
        c.puller = ObjectGuid::Empty;
        c.pullerIsTank = false;
        TC_LOG_INFO("playerbot.v2.raid", "[raid_siphon] group={} no Son of Hakkar in the fight: pulling the idle one {} y from Hakkar ({}), {} s before the siphon",
            plan.groupLow, uint32_t(hakkar->GetExactDist2d(best)), best->GetGUID().GetCounter(), (SiphonDueMs(c.lastSiphon) - t) / 1000u);
    }
    return best;
}

// The opener a bot pulls with (0: none it knows). Hunters first, then mages, then warlocks.
uint32_t PullSpell(Player* p, int& order)
{
    switch (p->GetClass())
    {
        case CLASS_HUNTER:  order = 0; return KnownRank(p, kArcaneShot);
        case CLASS_MAGE:    order = 1; return KnownRank(p, kFrostbolt);
        case CLASS_WARLOCK: order = 2; return KnownRank(p, kShadowBolt);
        default:            order = 3; return 0;
    }
}

} // namespace

void ZgMechanicsElect(Election& e, uint32_t now)
{
    Unit* hakkar = nullptr;
    uint32_t t = 0;
    SiphonClock* c = Clock(e.plan, now, hakkar, t);
    if (!c)
        return;
    Creature* son = SonToPull(e.plan, *c, hakkar, t);
    if (!son)
        return;
    // The off-tank takes the Son: a bot tank Hakkar is not on, the one nearest to the Son (kept while it stays one). It walks in and
    // attacks; its own taunt and threat keep the Son on it, and the DPS focus (SonToKill) follows once the Son is in the fight.
    Member* tank = nullptr;
    for (Member& m : e.plan.members)
    {
        if (!m.bot || !m.tank || !m.p->IsAlive() || hakkar->GetVictim() == m.p)
            continue;
        if (m.p->GetGUID() == c->puller && c->pullerIsTank)
        {
            tank = &m;
            break;
        }
        if (!tank || m.p->GetExactDist2d(son) < tank->p->GetExactDist2d(son))
            tank = &m;
    }
    if (tank)
    {
        if (c->puller != tank->p->GetGUID() || !c->pullerIsTank)
        {
            c->puller = tank->p->GetGUID();
            c->pullerIsTank = true;
            TC_LOG_INFO("playerbot.v2.raid", "[raid_siphon] group={} the off-tank {} takes the Son of Hakkar ({}) from {:.0f} y", e.plan.groupLow,
                tank->p->GetName(), son->GetGUID().GetCounter(), tank->p->GetExactDist2d(son));
        }
        if (RaidOrder* o = e.OrderOf(tank->p))
        {
            o->attack_target = son->GetGUID();
            e.busy.insert(tank->p->GetGUID().GetCounter());
            e.dump << "  Hakkar: " << tank->p->GetName() << " (off-tank) takes a Son of Hakkar\n";
        }
        return;
    }
    c->pullerIsTank = false;
    // No off-tank alive: a ranged bot pulls with one shot. The puller: kept while it can still pull, busy or not (its own walk into reach makes it "busy" for the election, which hands every
    // bot with a move order no job; checking that here swapped the puller each pass, the one walking in lost the job to the other hunter
    // and the other's walk took it back: sweep-zg-1109c, Mchunta and Mchuntb "pull" 122 times in turn, no shot fired). Else the free
    // ranged DPS bot of the first class in the order, nearest to the Son, that is not Hakkar's target and carries no Corrupted Blood (it
    // would bring the plague into the crowd).
    auto const canPull = [&](Member const& m)
    {
        int order = 3;
        return m.bot && !m.tank && !m.healer && m.p->IsAlive() && hakkar->GetVictim() != m.p && !m.p->HasAura(kCorruptedBlood) &&
               PullSpell(m.p, order) != 0;
    };
    Member* puller = nullptr;
    for (Member& m : e.plan.members)
        if (m.p->GetGUID() == c->puller && canPull(m))
            puller = &m;
    int bestOrder = 3;
    float bestYards = 0.0f;
    if (!puller)
        for (Member& m : e.plan.members)
        {
            if (!canPull(m) || e.busy.count(m.p->GetGUID().GetCounter()))
                continue;
            int order = 3;
            PullSpell(m.p, order);
            float const yards = m.p->GetExactDist2d(son);
            if (!puller || order < bestOrder || (order == bestOrder && yards < bestYards))
            {
                puller = &m;
                bestOrder = order;
                bestYards = yards;
            }
        }
    if (!puller)
        return;
    if (c->puller != puller->p->GetGUID())
    {
        c->puller = puller->p->GetGUID();
        TC_LOG_INFO("playerbot.v2.raid", "[raid_siphon] group={} {} pulls the Son of Hakkar ({}) from {:.0f} y", e.plan.groupLow, puller->p->GetName(),
            son->GetGUID().GetCounter(), puller->p->GetExactDist2d(son));
    }
    int order = 3;
    uint32_t const spell = PullSpell(puller->p, order);
    if (e.busy.count(puller->p->GetGUID().GetCounter()) || !Ready(puller->p, spell, son, true))
        return;   // walking in (ZgMechanicsMove), or out of reach or sight: the walk goes on
    RaidOrder* o = e.OrderOf(puller->p);
    if (!o)
        return;
    o->cast_kind = RaidOrder::CastKind::Nuke;
    o->cast_spell = spell;
    o->cast_target = son->GetGUID();
    e.busy.insert(puller->p->GetGUID().GetCounter());
    e.dump << "  Hakkar: " << puller->p->GetName() << " pulls a Son of Hakkar\n";
}

Unit* ZgForcedFocus(Plan const& plan, uint32_t now)
{
    Unit* hakkar = nullptr;
    uint32_t t = 0;
    SiphonClock* c = Clock(plan, now, hakkar, t);
    return c ? SonToKill(plan, *c, t) : nullptr;
}

bool ZgMechanicsMove(Plan const& plan, Member const& self, uint32_t now, RaidOrder& out)
{
    Unit* hakkar = nullptr;
    uint32_t t = 0;
    SiphonClock* c = Clock(plan, now, hakkar, t);
    if (!c)
        return false;
    Player* bot = self.p;
    // The puller of an idle Son walks into reach and sight of it (the shot itself is the elected job).
    if (!c->puller.IsEmpty() && c->puller == bot->GetGUID() && !c->pullerIsTank)
        if (Creature* pull = SonToPull(plan, *c, hakkar, t))
        {
            if (bot->IsWithinDistInMap(pull, kPullReach + 2.0f) && bot->IsWithinLOSInMap(pull))
                return false;
            float const yards = bot->GetExactDist2d(pull);
            float const k = yards > kPullReach ? (yards - kPullReach) / yards : 0.5f;
            float const x = bot->GetPositionX() + (pull->GetPositionX() - bot->GetPositionX()) * k;
            float const y = bot->GetPositionY() + (pull->GetPositionY() - bot->GetPositionY()) * k;
            return SetMove(out, x, y, pull->GetPositionZ(), "pull a Son of Hakkar");
        }
    Unit* son = SonToKill(plan, *c, t);
    if (!son)
        return false;
    bool const excluded = hakkar->GetVictim() == bot || bot->HasAura(kCorruptedBlood) || son->GetVictim() == bot;
    float const yards = bot->GetExactDist2d(son);
    if (yards <= kGatherYards && !bot->HasAura(kPoisonousBlood) && !excluded && son->GetHealthPct() <= kGatherBelowPct)
    {
        out.at_post = true;   // on it: stay for its death
        return true;
    }
    if (!Gathers(bot->HasAura(kPoisonousBlood), excluded, son->GetHealthPct(), yards))
        return false;
    // Toward the Son, stopping short of it (on the bot's side, so the crowd does not all stand on one spot behind it).
    float const k = (yards - (kGatherYards - 1.0f)) / yards;
    float const x = bot->GetPositionX() + (son->GetPositionX() - bot->GetPositionX()) * k;
    float const y = bot->GetPositionY() + (son->GetPositionY() - bot->GetPositionY()) * k;
    return SetMove(out, x, y, son->GetPositionZ(), "gather on Son of Hakkar");
}

} // namespace Playerbot::Raid::detail
