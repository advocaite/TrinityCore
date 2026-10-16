#include "RaidEncounters.h"

#include <algorithm>
#include <map>
#include <memory>
#include <mutex>

namespace Playerbot::Raid {

namespace {

std::map<uint32_t, std::unique_ptr<RaidRules>>& Registry()
{
    static std::map<uint32_t, std::unique_ptr<RaidRules>> rules;
    static std::once_flag once;
    std::call_once(once, []
    {
        auto molten = std::make_unique<RaidRules>();
        molten->name = "Molten Core";
        RegisterMoltenCoreMovement(*molten);
        RegisterMoltenCore(*molten);
        rules[409] = std::move(molten);

        auto blackwing = std::make_unique<RaidRules>();
        blackwing->name = "Blackwing Lair";
        RegisterBlackwingMovement(*blackwing);
        RegisterBlackwingLair(*blackwing);
        rules[469] = std::move(blackwing);

        auto onyxia = std::make_unique<RaidRules>();
        onyxia->name = "Onyxia's Lair";
        RegisterOnyxia(*onyxia);
        // Her frontal cones (Flame Breath, Tail Sweep) are in RegisterOnyxia; a second copy here made the tank turn her again.
        // Phase 3 Bellowing Roar: AoE fear, dispelled off whoever a dispeller can reach. This core casts 22686 (classic_boss_onyxia.cpp:53;
        // vanilla 18431 is not in the 1.60 client), so the old 18431 rule never matched. No order when the data gives it no dispel type.
        onyxia->dispel.push_back({ 22686, DispelKind::Magic });
        rules[249] = std::move(onyxia);

        auto aq40 = std::make_unique<RaidRules>();
        aq40->name = "Temple of Ahn'Qiraj";
        RegisterTempleOfAhnQiraj(*aq40);
        rules[531] = std::move(aq40);

        auto naxx = std::make_unique<RaidRules>();
        naxx->name = "Naxxramas";
        RegisterNaxxramas(*naxx);
        rules[533] = std::move(naxx);
    });
    return rules;
}

} // namespace

RaidRules const* RulesForMap(uint32_t mapId)
{
    auto& registry = Registry();
    auto it = registry.find(mapId);
    return it == registry.end() ? nullptr : it->second.get();
}

bool OwnsAura(uint32_t mapId, uint32_t aura)
{
    RaidRules const* rules = RulesForMap(mapId);
    if (!rules)
        return false;
    for (Dispel const& rule : rules->dispel)
        if (rule.aura == aura)
            return true;
    for (ControlledMember const& rule : rules->controlledMember)
        if (rule.aura == aura)
            return true;
    return std::find(rules->noDispel.begin(), rules->noDispel.end(), aura) != rules->noDispel.end();
}

bool InterruptFirst(uint32_t mapId, uint32_t spell)
{
    RaidRules const* rules = RulesForMap(mapId);
    return rules && std::find(rules->interruptFirst.begin(), rules->interruptFirst.end(), spell) != rules->interruptFirst.end();
}

bool NoTank(uint32_t mapId, uint32_t entry)
{
    RaidRules const* rules = RulesForMap(mapId);
    return rules && std::find(rules->noTank.begin(), rules->noTank.end(), entry) != rules->noTank.end();
}

uint32_t TankOrder(uint32_t mapId, uint32_t entry)
{
    RaidRules const* rules = RulesForMap(mapId);
    if (!rules)
        return UINT32_MAX;
    auto const it = std::find(rules->tankOrder.begin(), rules->tankOrder.end(), entry);
    return it == rules->tankOrder.end() ? UINT32_MAX : uint32_t(it - rules->tankOrder.begin());
}

bool TankLast(uint32_t mapId, uint32_t entry)
{
    RaidRules const* rules = RulesForMap(mapId);
    return rules && std::find(rules->tankLast.begin(), rules->tankLast.end(), entry) != rules->tankLast.end();
}

bool Retaunts(uint32_t mapId, uint32_t entry)
{
    RaidRules const* rules = RulesForMap(mapId);
    return rules && std::find(rules->retaunt.begin(), rules->retaunt.end(), entry) != rules->retaunt.end();
}

float ThreatShareAfterDrop(RaidRules const& rules, uint32_t entry)
{
    for (ThreatDrop const& rule : rules.threatDrop)
        if (rule.enemy == entry)
            return std::clamp(1.0f - rule.percent / 100.0f, 0.1f, 1.0f);
    return 1.0f;
}

float ThreatShareAfterDrop(uint32_t mapId, uint32_t entry)
{
    RaidRules const* rules = RulesForMap(mapId);
    return rules ? ThreatShareAfterDrop(*rules, entry) : 1.0f;
}

int PriorityFor(RaidRules const& rules, uint32_t entry)
{
    for (Priority const& rule : rules.priority)
        if (rule.enemy == entry && rule.who == Who::Anyone && !rule.gateEnemy)
            return rule.value;
    return 1;
}

std::vector<uint32_t> KillOrder(uint32_t mapId)
{
    std::vector<uint32_t> order;
    RaidRules const* rules = RulesForMap(mapId);
    if (!rules)
        return order;
    std::vector<Priority> ranked;
    for (Priority const& rule : rules->priority)
        if (rule.value >= 2 && rule.who == Who::Anyone && !rule.gateEnemy)
            ranked.push_back(rule);
    std::stable_sort(ranked.begin(), ranked.end(), [](Priority const& a, Priority const& b) { return a.value > b.value; });
    for (Priority const& rule : ranked)
        order.push_back(rule.enemy);
    return order;
}

} // namespace Playerbot::Raid
