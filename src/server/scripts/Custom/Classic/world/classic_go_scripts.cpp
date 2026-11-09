/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

// Classic 1.60 port of VMaNGOS src/scripts/world/go_scripts.cpp (ScriptDev2 lineage, GPL-2)
// Field Repair Bot 74A schematic: teaches spell 22704 to engineers (skill >= 300)

#include "ScriptMgr.h"
#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "Battleground.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "Player.h"
#include "SharedDefines.h"
#include "Creature.h"
#include "RestMgr.h"
#include "SpellAuraEffects.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "UpdateFields.h"
#include <array>
#include <list>
#include <set>

/*######
## go_field_repair_bot_74A
######*/

enum FieldRepairBot74A
{
    FIELD_REPAIR_BOT_SPELL_LEARNED  = 22704,    // Field Repair Bot 74A
    FIELD_REPAIR_BOT_SPELL_LEARN    = 22864,    // teaches 22704
    FIELD_REPAIR_BOT_SKILL_REQUIRED = 300
};

struct classic_go_field_repair_bot_74A : public GameObjectAI
{
    classic_go_field_repair_bot_74A(GameObject* go) : GameObjectAI(go) { }

    bool OnGossipHello(Player* player) override
    {
        if (player->HasSkill(SKILL_ENGINEERING) && player->GetBaseSkillValue(SKILL_ENGINEERING) >= FIELD_REPAIR_BOT_SKILL_REQUIRED
            && !player->HasSpell(FIELD_REPAIR_BOT_SPELL_LEARNED))
            player->CastSpell(player, FIELD_REPAIR_BOT_SPELL_LEARN, false);
        return true;
    }
};

/*######
## classic_spell_campfire_rest (1289723)
######*/

// Classic 1.60 (WoW Forever) camping: sitting near a campfire starts this 60 s rest (WorldSession::HandleStandStateChangeOpcode);
// when it runs out the player gets the "Boosted ..." buff of every camp feature near where they sit (client spell texts: "Players
// sitting or crafting nearby for 1 min will gain benefits from other camp features") and, still sitting by the fire, the next rest
// starts (ymir sniff of the official beta). Standing up removes it early, so the minute has to be sat out in one go.
enum CampfireRest
{
    SPELL_CAMPFIRE_REST         = 1289723,
    SPELL_BOOSTED_REST          = 1229451,  // Camp Tent
    SPELL_BOOSTED_CRIT          = 1229519,  // Camp Chair
    SPELL_BOOSTED_ATTACK_POWER  = 1230164,  // Lodestone
    SPELL_BOOSTED_MANA_REGEN    = 1230587,  // Mana Well
    SPELL_EXTRA_STRENGTH        = 1230172,  // Sharpening Wheel
    SPELL_BOOSTED_INTELLECT     = 1229513,  // Incense Candle
    SPELL_BOOSTED_SPIRIT        = 1229718,  // Faction Banner
    SPELL_BOOSTED_STAMINA       = 1230124,  // First Aid Kit
    SPELL_BOOSTED_STATS_PCT     = 1230098,  // Fish Bowl
    SPELL_BOOSTED_STATS_LUTE    = 1230653,  // Enchanted Lute: armor, all stats, all resistances
    SPELL_CAMP_BENEFITS         = 1229741,  // visible summary buff ("Gained the following camp benefits: ...")
    SPELL_FOCUS_CAMPFIRE        = 4
};

static constexpr float CAMPFIRE_RANGE = 15.0f;      // sitting this close to the fire (same as HandleStandStateChangeOpcode)
// the Basic Campfire of the adventurer camps (Goldshire, Brill, Kharanos, Razor Hill, Bloodhoof, Dolanaar, Zephras Isle): a fixed
// spawn, unlike a player's fire (summoned, no spawn id)
static constexpr uint32 GO_BASIC_CAMPFIRE = 529161;
// features this close to the resting player count: Camp Benefits 1229741 finds them with its area effects 1 and 2 (creatures and
// gameobjects round the caster's spot, radius index 9 = 20 yards), not round the fire
static constexpr float CAMP_FEATURE_RANGE = 20.0f;

// Camp features -> their "Boosted ..." buff. The features are the gameobjects summoned by the placement spells (1307229 Camp Chair,
// 1307254 Lodestone, ... effect 50); only Chair, Lodestone and Mana Well have templates so far (sniffed next to the camping
// trainer's fire), the others need a sniff of placing them. The trainer's camp has its tent as a creature.
struct CampFeature
{
    uint32 Entry;
    bool IsCreature;
    uint32 Buff;
};

static constexpr CampFeature CampFeatures[] =
{
    { 263398, true,  SPELL_BOOSTED_REST },          // Camp Tent (creature, camping trainer's camp)
    { 528996, false, SPELL_BOOSTED_REST },          // Camp Tent
    { 612275, false, SPELL_BOOSTED_CRIT },          // Camp Chair
    { 651956, false, SPELL_BOOSTED_ATTACK_POWER },  // Lodestone
    { 651948, false, SPELL_BOOSTED_MANA_REGEN },    // Mana Well
    { 651950, false, SPELL_EXTRA_STRENGTH },        // Sharpening Wheel
    { 651955, false, SPELL_BOOSTED_INTELLECT },     // Incense Candle
    { 612350, false, SPELL_BOOSTED_SPIRIT },        // Faction Banner
    { 612351, false, SPELL_BOOSTED_SPIRIT },        // Faction Banner
    { 651953, false, SPELL_BOOSTED_STAMINA },       // First Aid Kit
    { 651954, false, SPELL_BOOSTED_STATS_PCT },     // Fish Bowl
    { 651952, false, SPELL_BOOSTED_STATS_LUTE },    // Enchanted Lute
    // tier 2/3 features that "provide all of the benefits of" the one they replace (70790 client texts of their placement spells;
    // gameobjects from the spells' effect 50). Without these, placing one over the lower feature lost its buff.
    { 612120, false, SPELL_BOOSTED_MANA_REGEN },    // Fermenter (1307242), over a Mana Well
    { 612092, false, SPELL_BOOSTED_STATS_PCT },     // Fishing Hut (1307246), over a Fish Bowl
    { 612090, false, SPELL_BOOSTED_STATS_PCT },     // Fishing Rack (1307247), over a Fish Bowl
    { 612087, false, SPELL_BOOSTED_INTELLECT },     // Greenhouse (1307248), over an Incense Candle
    { 612136, false, SPELL_EXTRA_STRENGTH },        // Master Forge (1307261), over a Sharpening Wheel
    { 612110, false, SPELL_BOOSTED_STAMINA },       // Plague Doctor's Laboratory (1307265), over a First Aid Kit
};

// The buffs scale with level in steps (client tooltips: "$?$PL<12[3]?$PL<24[8]...[$1230124s1]"); the spell's own value is the
// highest step. Returns the value for 'level', or -1 to keep the spell's value.
struct CampTier { uint8 BelowLevel; int32 Value; };

static int32 CampTierValue(uint8 level, std::initializer_list<CampTier> tiers)
{
    for (CampTier const& tier : tiers)
        if (level < tier.BelowLevel)
            return tier.Value;
    return -1;
}

static void CastCampBuff(Unit* target, uint32 buff)
{
    uint8 level = target->GetLevel();
    CastSpellExtraArgs args(TRIGGERED_FULL_MASK);
    auto setValue = [&](uint8 effIndex, int32 value)
    {
        if (value >= 0)
            args.AddSpellMod(SpellValueModFloat(SPELLVALUE_BASE_POINT0 + effIndex), float(value));
    };

    switch (buff)
    {
        case SPELL_BOOSTED_STAMINA:
            setValue(0, CampTierValue(level, { { 12, 3 }, { 24, 8 }, { 36, 21 }, { 48, 34 }, { 60, 45 } }));
            break;
        case SPELL_BOOSTED_ATTACK_POWER:
            setValue(0, CampTierValue(level, { { 12, 12 }, { 22, 20 }, { 32, 32 }, { 42, 49 }, { 52, 67 } }));
            break;
        case SPELL_BOOSTED_MANA_REGEN:
            setValue(0, CampTierValue(level, { { 24, 10 }, { 34, 15 }, { 44, 20 }, { 54, 24 } }));
            break;
        case SPELL_EXTRA_STRENGTH:
            setValue(0, CampTierValue(level, { { 24, 6 }, { 38, 11 }, { 52, 20 } }));
            break;
        case SPELL_BOOSTED_INTELLECT:
            setValue(0, CampTierValue(level, { { 14, 2 }, { 28, 6 }, { 42, 12 }, { 56, 18 } }));
            break;
        case SPELL_BOOSTED_SPIRIT:
            setValue(0, CampTierValue(level, { { 40, 14 }, { 50, 19 }, { 60, 27 } }));
            break;
        case SPELL_BOOSTED_STATS_LUTE:
        {
            setValue(0, CampTierValue(level, { { 10, 28 }, { 20, 71 }, { 30, 114 }, { 40, 163 }, { 50, 211 }, { 60, 260 } }));
            setValue(1, CampTierValue(level, { { 10, 0 }, { 20, 2 }, { 30, 4 }, { 40, 7 }, { 50, 9 }, { 60, 12 } }));
            int32 resistance = CampTierValue(level, { { 30, 0 }, { 40, 6 }, { 50, 12 }, { 60, 16 } });
            for (uint8 i = 2; i <= 7; ++i)
                setValue(i, resistance);
            break;
        }
        default:    // Boosted Rest, Critical Chance and Stats (%) don't scale
            break;
    }
    target->CastSpell(target, buff, args);
}

// The Great Outdoors asks to rest at the adventurer's camp for Boosted Rest. That camp's tent is part of the map, so no Camp Tent
// object stands there: the server spawns of it floated over the map's tent and were removed (2026_11_05_75, 2026_11_07_80), and
// from then on the camp gave only the 60 s rest, never Boosted Rest (Discord, ErikaST and Joy 2026-11-08: Goldshire and Brill
// since 0.1.32.1; a player's own camp with a tent still worked). A fixed Basic Campfire spawn within reach counts as that camp.
static bool IsAtAdventurerCamp(Unit* unit)
{
    std::list<GameObject*> fires;
    unit->GetGameObjectListWithEntryInGrid(fires, GO_BASIC_CAMPFIRE, CAMPFIRE_RANGE);
    for (GameObject* fire : fires)
        if (fire->GetSpawnId())
            return true;
    return false;
}

// the campfire (spell focus 4) the player is resting at, nullptr if none is close enough
static GameObject* FindCampfire(Unit* unit)
{
    GameObject* fire = unit->FindNearestGameObjectOfType(GAMEOBJECT_TYPE_SPELL_FOCUS, CAMPFIRE_RANGE);
    return fire && fire->GetGOInfo()->spellFocus.spellFocusType == SPELL_FOCUS_CAMPFIRE ? fire : nullptr;
}

class classic_spell_campfire_rest : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_BOOSTED_REST });
    }

    void AfterRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;

        Unit* target = GetTarget();
        GameObject* fire = FindCampfire(target);
        if (!fire)
            return;

        std::set<uint32> buffs;
        for (CampFeature const& feature : CampFeatures)
        {
            bool nearby = feature.IsCreature ? target->FindNearestCreature(feature.Entry, CAMP_FEATURE_RANGE) != nullptr
                : target->FindNearestGameObject(feature.Entry, CAMP_FEATURE_RANGE) != nullptr;
            if (nearby && sSpellMgr->GetSpellInfo(feature.Buff, DIFFICULTY_NONE))
                buffs.insert(feature.Buff);
        }
        if (IsAtAdventurerCamp(target) && sSpellMgr->GetSpellInfo(SPELL_BOOSTED_REST, DIFFICULTY_NONE))
            buffs.insert(SPELL_BOOSTED_REST);
        for (uint32 buff : buffs)
            CastCampBuff(target, buff);
        // the Boosted buffs are hidden auras (Attributes 0x80); the visible buff is Camp Benefits, whose tooltip lists every one of them
        // the player has (official beta sniff: both in the same aura updates). Its other effects are area dummies, so add the aura only.
        if (!buffs.empty() && sSpellMgr->GetSpellInfo(SPELL_CAMP_BENEFITS, DIFFICULTY_NONE))
            target->AddAura(SPELL_CAMP_BENEFITS, target);

        if (target->IsSitState())
            target->CastSpell(target, SPELL_CAMPFIRE_REST, true);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(classic_spell_campfire_rest::AfterRemove, EFFECT_0, SPELL_AURA_ANY, AURA_EFFECT_HANDLE_REAL);
    }
};

/*######
## classic_spell_camp_feature_placement (the camp feature spells, "Requires a Campfire nearby")
######*/

// A player's campfire "allows ... the placement of up to $s1 additional camp features" (70707 client texts: Basic Campfire 1307227
// effect 0 = 3, Journeyman 1307252 = 5, Expert 1307237 = 10). Nothing counted them, so any number could be placed (Azeldof,
// 2026-11-02). A feature counts for the fire it stands near. The higher tier features "may be placed over" a lower one "to replace
// it": that one is removed and the count stays the same. A campfire that is part of the world (no spell made it) has no limit.
struct CampPlacement
{
    uint32 SpellId;
    std::array<uint32, 2> Replaces;     // camp feature gameobjects this one may be placed over
};

static constexpr CampPlacement CampPlacements[] =
{
    { 1307229, { } },                   // Camp Chair
    { 1307230, { } },                   // Camp Tent
    { 1307234, { } },                   // Enchanted Lute
    { 1307239, { } },                   // Faction Banner
    { 1307240, { } },                   // Faction Banner
    { 1307244, { } },                   // First Aid Kit
    { 1307245, { } },                   // Fish Bowl
    { 1307251, { } },                   // Incense Candle
    { 1307254, { } },                   // Lodestone
    { 1307259, { } },                   // Mana Well
    { 1307266, { } },                   // Reagent Bot
    { 1307223, { 651952 } },            // Arcane Salvager, over an Enchanted Lute
    { 1307242, { 651948 } },            // Fermenter, over a Mana Well
    { 1307243, { 612275 } },            // Field Guide, over a Camp Chair
    { 1307246, { 651954 } },            // Fishing Hut, over a Fish Bowl
    { 1307247, { 651954 } },            // Fishing Rack, over a Fish Bowl
    { 1307248, { 651955 } },            // Greenhouse, over an Incense Candle
    { 1307255, { 612350, 612351 } },    // Loom, over a Faction Banner
    { 1307261, { 651950 } },            // Master Forge, over a Sharpening Wheel
    { 1307264, { 651956 } },            // Molten Foundry, over a Lodestone
    { 1307265, { 651953 } },            // Plague Doctor's Laboratory, over a First Aid Kit
};

static CampPlacement const* FindCampPlacement(uint32 spellId)
{
    for (CampPlacement const& placement : CampPlacements)
        if (placement.SpellId == spellId)
            return &placement;
    return nullptr;
}

class classic_spell_camp_feature_placement : public SpellScript
{
    // the player's feature next to 'fire' that this one is placed over, nullptr if none
    GameObject* FindReplaced(GameObject* fire) const
    {
        CampPlacement const* placement = FindCampPlacement(GetSpellInfo()->Id);
        if (!placement)
            return nullptr;

        for (uint32 entry : placement->Replaces)
            if (entry)
                if (GameObject* old = fire->FindNearestGameObject(entry, CAMP_FEATURE_RANGE))
                    if (old->GetSpellId())
                        return old;
        return nullptr;
    }

    SpellCastResult CheckCast()
    {
        GameObject* fire = FindCampfire(GetCaster());
        if (!fire || !fire->GetSpellId())   // no fire: the spell's own focus check says so; world campfires have no limit
            return SPELL_CAST_OK;

        SpellInfo const* fireSpell = sSpellMgr->GetSpellInfo(fire->GetSpellId(), DIFFICULTY_NONE);
        if (!fireSpell || fireSpell->GetEffects().empty())
            return SPELL_CAST_OK;

        int32 limit = fireSpell->GetEffect(EFFECT_0).CalcValue();
        if (limit <= 0 || FindReplaced(fire))
            return SPELL_CAST_OK;

        std::list<GameObject*> objects;
        fire->GetGameObjectListWithEntryInGrid(objects, 0, CAMP_FEATURE_RANGE);
        int32 placed = 0;
        for (GameObject const* object : objects)
            if (object->isSpawned() && FindCampPlacement(object->GetSpellId()))
                ++placed;

        return placed >= limit ? SPELL_FAILED_TOO_MANY_OF_ITEM : SPELL_CAST_OK;
    }

    void RemoveReplaced()
    {
        if (GameObject* fire = FindCampfire(GetCaster()))
            if (GameObject* old = FindReplaced(fire))
                old->DespawnOrUnsummon();
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(classic_spell_camp_feature_placement::CheckCast);
        AfterCast += SpellCastFn(classic_spell_camp_feature_placement::RemoveReplaced);
    }
};

/*######
## classic_spell_boosted_rest (1229451)
######*/

// Boosted Rest, the Camp Tent's benefit: "increase Rested experience to <effect value>% of a level. No effect if Rested experience
// already exceeds that value." (client spell text; effect 0 is a dummy with value 5)
class classic_spell_boosted_rest : public SpellScript
{
    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Player* player = Object::ToPlayer(GetHitUnit());
        if (!player)
            return;

        float wanted = float(*player->m_activePlayerData->NextLevelXP) * float(GetEffectValue()) / 100.0f;
        if (player->GetRestMgr().GetRestBonus(REST_TYPE_XP) < wanted)
            player->GetRestMgr().SetRestBonus(REST_TYPE_XP, wanted);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_boosted_rest::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

/*######
## classic_go_camp_chair (612275)
######*/

// Sitting in a Camp Chair goes through GameObject::Use, not CMSG_STAND_STATE_CHANGE, so start the campfire rest here as well;
// returning false lets the chair seat the player as usual (standing up later removes the rest in HandleStandStateChangeOpcode)
struct classic_go_camp_chair : public GameObjectAI
{
    classic_go_camp_chair(GameObject* go) : GameObjectAI(go) { }

    bool OnGossipHello(Player* player) override
    {
        if (!player->HasAura(SPELL_CAMPFIRE_REST) && FindCampfire(player))
            player->CastSpell(player, SPELL_CAMPFIRE_REST, true);
        return false;
    }
};

/*######
## classic_spell_plainsrunning (1259918)
######*/

// Classic 1.60 Tauren racial: "Gain 2% increased movement speed every 5 sec spent moving, up to a maximum of 60% increase. Taking
// damage or standing still will reduce this effect." (WoW Forever beta notes 2026-10-26: was 1% up to 30%, and no longer adds mounted
// speed; values in the hotfixes of 2026_10_27_05). The racial is a 1 s periodic dummy (effect 0, the percent per step) with the step
// seconds and the maximum percent in effects 1 and 2; the speed is the stacking aura Plainsrunning (1299038, +2% per stack), so the
// stack cap is the maximum divided by the step. How fast it decays is not in the spell data: here one stack per second standing still
// and five stacks when hit.
// Blizzard hotfix 2026-11-05: Plainsrunning no longer stacks, also not in a battleground's preparation, and carrying a flag cancels
// its stacks. Here: no stacks are gained while the battleground waits for its start, and a flag carrier (Warsong Gulch Warsong Flag
// 23333 / Silverwing Flag 23335) loses them all and gains none.
enum Plainsrunning
{
    SPELL_PLAINSRUNNING_SPEED       = 1299038,
    PLAINSRUNNING_STACKS_LOST_ON_HIT = 5,
    SPELL_WARSONG_FLAG              = 23333,
    SPELL_SILVERWING_FLAG           = 23335
};

class classic_spell_plainsrunning : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ SPELL_PLAINSRUNNING_SPEED }) && ValidateSpellEffect({ { spellInfo->Id, EFFECT_2 } });
    }

    void RemoveStacks(Unit* target, int32 count)
    {
        Aura* speed = target->GetAura(SPELL_PLAINSRUNNING_SPEED);
        if (!speed)
            return;

        if (speed->GetStackAmount() <= count)
            speed->Remove();
        else
            speed->ModStackAmount(-count);
    }

    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        Unit* target = GetTarget();
        int32 secondsPerStack = std::max(1, int32(GetEffect(EFFECT_1)->GetAmount()));
        int32 maxStacks = std::max(1, int32(GetEffect(EFFECT_2)->GetAmount()) / std::max(1, int32(GetEffect(EFFECT_0)->GetAmount())));

        uint64 health = target->GetHealth();
        bool hit = health < _lastHealth;
        _lastHealth = health;

        if (target->HasAura(SPELL_WARSONG_FLAG) || target->HasAura(SPELL_SILVERWING_FLAG))
        {
            _movingSeconds = 0;
            target->RemoveAurasDueToSpell(SPELL_PLAINSRUNNING_SPEED);
            return;
        }

        if (Player* player = target->ToPlayer())
        {
            if (Battleground* bg = player->GetBattleground(); bg && bg->GetStatus() == STATUS_WAIT_JOIN)
            {
                _movingSeconds = 0;
                return;
            }
        }

        if (hit)
        {
            _movingSeconds = 0;
            RemoveStacks(target, PLAINSRUNNING_STACKS_LOST_ON_HIT);
            return;
        }

        if (!target->isMoving())
        {
            _movingSeconds = 0;
            RemoveStacks(target, 1);
            return;
        }

        if (++_movingSeconds < secondsPerStack)
            return;

        _movingSeconds = 0;
        if (Aura* speed = target->GetAura(SPELL_PLAINSRUNNING_SPEED))
        {
            if (speed->GetStackAmount() < maxStacks)
                speed->ModStackAmount(1);
            else
                speed->RefreshDuration();
        }
        else
            target->CastSpell(target, SPELL_PLAINSRUNNING_SPEED, true);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(classic_spell_plainsrunning::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }

    uint32 _movingSeconds = 0;
    uint64 _lastHealth = 0;
};

/*######
## classic_at_energizing_vortex (area trigger of Energizing Winds 1299003, create properties 43253)
######*/

// Classic 1.60 (WoW Forever): touching an Energizing Vortex gives Blessing of Zephras (run speed +40% for 5 min). The player casts
// it on himself: an area trigger action would cast it as the vortex, and TC removes such auras when the unit leaves the (moving)
// trigger, a second later.
enum EnergizingVortex
{
    SPELL_BLESSING_OF_ZEPHRAS = 1258510
};

struct classic_at_energizing_vortex : AreaTriggerAI
{
    explicit classic_at_energizing_vortex(AreaTrigger* areaTrigger) : AreaTriggerAI(areaTrigger) { }

    void OnUnitEnter(Unit* unit) override
    {
        if (Player* player = unit->ToPlayer())
            if (!player->IsInCombat())
                player->CastSpell(player, SPELL_BLESSING_OF_ZEPHRAS, true);
    }
};

/*######
## classic_at_rohashi_wind (static area trigger at the edge of the Rohashi Spires platform)
######*/

// Classic 1.60 (WoW Forever), quest Confront Lorthuna (92646, official beta sniff 70205): stepping into the wind at the edge of the
// platform the Portal To Rohashi Spires leads to launches the player over to Lorthuna (1256704: jump to 3013.0 118.4 1164.6).
enum RohashiWind
{
    SPELL_ROHASHI_WIND_LAUNCH = 1256704
};

struct classic_at_rohashi_wind : AreaTriggerAI
{
    explicit classic_at_rohashi_wind(AreaTrigger* areaTrigger) : AreaTriggerAI(areaTrigger) { }

    void OnUnitEnter(Unit* unit) override
    {
        if (Player* player = unit->ToPlayer())
            player->CastSpell(player, SPELL_ROHASHI_WIND_LAUNCH, true);
    }
};

/*######
## classic_at_secluded_house (static area trigger at the turncoat's house in Shen'dar Highlands)
######*/

// Classic 1.60 (WoW Forever), quest The Turncoat (92643, official beta sniff 70205): reaching the secluded house gives "Find the
// secluded house in Shen'dar Highlands" (255013).
enum SecludedHouse
{
    QUEST_THE_TURNCOAT          = 92643,
    NPC_SECLUDED_HOUSE_CREDIT   = 255013
};

struct classic_at_secluded_house : AreaTriggerAI
{
    explicit classic_at_secluded_house(AreaTrigger* areaTrigger) : AreaTriggerAI(areaTrigger) { }

    void OnUnitEnter(Unit* unit) override
    {
        if (Player* player = unit->ToPlayer())
            if (player->GetQuestStatus(QUEST_THE_TURNCOAT) == QUEST_STATUS_INCOMPLETE)
                player->KilledMonsterCredit(NPC_SECLUDED_HOUSE_CREDIT);
    }
};

/*######
## classic_event_skycutter_arrival (taxi path arrival events of the Skycutters)
######*/

// Classic 1.60 (WoW Forever): when a Skycutter docks, the dockmaster there announces it (official beta sniff, build 70170). The
// events are the ArrivalEventIDs of the dock nodes in TaxiPathNode.db2; the Mulgore Skycutter's docks (105969 Zephras Isle,
// 105967 Mulgore) were not heard in the sniffs yet.
struct SkycutterAnnouncement
{
    uint32 EventId;
    uint32 Creature;
    char const* Text;
};

static constexpr SkycutterAnnouncement SkycutterAnnouncements[] =
{
    { 105988, 275269, "The skycutter bound for Dalaran City has just arrived. All aboard for Dalaran City!" },             // Zephras Isle dock, High Order Dockmaster
    { 103315, 256306, "The skycutter to Zephras Isle has just arrived. Please watch your step when boarding the vessel." }, // Dalaran (Lordamere Lake) dock, Arcanist Laurain
};

class classic_event_skycutter_arrival : public EventScript
{
public:
    classic_event_skycutter_arrival() : EventScript("classic_event_skycutter_arrival") { }

    void OnTrigger(WorldObject* object, WorldObject* invoker, uint32 eventId) override
    {
        WorldObject* transport = object ? object : invoker;
        if (!transport)
            return;

        for (SkycutterAnnouncement const& announcement : SkycutterAnnouncements)
            if (announcement.EventId == eventId)
                if (Creature* dockmaster = transport->FindNearestCreature(announcement.Creature, 100.0f))
                    dockmaster->Say(announcement.Text, LANG_UNIVERSAL);
    }
};

void AddSC_classic_go_scripts()
{
    new classic_event_skycutter_arrival();
    RegisterSpellScript(classic_spell_plainsrunning);
    RegisterAreaTriggerAI(classic_at_energizing_vortex);
    RegisterAreaTriggerAI(classic_at_rohashi_wind);
    RegisterAreaTriggerAI(classic_at_secluded_house);
    RegisterGameObjectAI(classic_go_field_repair_bot_74A);
    RegisterGameObjectAI(classic_go_camp_chair);
    RegisterSpellScript(classic_spell_campfire_rest);
    RegisterSpellScript(classic_spell_boosted_rest);
    RegisterSpellScript(classic_spell_camp_feature_placement);
}
