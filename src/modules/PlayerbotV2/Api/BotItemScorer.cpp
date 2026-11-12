#include "BotItemScorer.h"
#include "ClassicSpec.h"
#include "ItemTemplate.h"
#include "Item.h"
#include "Player.h"
#include "DBCEnums.h"
#include "SharedDefines.h"
#include "ItemEnchantmentMgr.h"   // GetRandomPropertyPoints
#include "SpellInfo.h"
#include "SpellMgr.h"
#include <algorithm>
#include <cmath>

namespace Playerbot::Gear {

int32 EffectiveItemLevelForLevel(ItemTemplate const* tpl, uint8 level)
{
    if (!tpl)
        return 0;
    // Mirror Item::GetItemLevel(owner) but without a live Item: initialize a
    // stack BonusData from the template (curve ids + base ilvl) and call the
    // static overload. fixedLevel/min/max/azerite all 0 + pvp false, matching a
    // normal sub-max wearer (the under-gearing case). Items with no scaling
    // curve fall through to the static base ilvl unchanged.
    BonusData bonus;
    bonus.Initialize(tpl);
    return int32(Item::GetItemLevel(tpl, bonus, level, /*fixedLevel*/ 0,
        /*minItemLevel*/ 0, /*minItemLevelCutoff*/ 0, /*maxItemLevel*/ 0,
        /*pvpBonus*/ false, /*azeriteLevel*/ 0, /*overrideContentTuningId*/ 0));
}

// Declared in BotItemScorer.h - see there for why this is level-independent.
ItemSubclassArmor PreferredArmorForClass(uint8 cls)
{
    switch (cls)
    {
        case CLASS_WARRIOR:
        case CLASS_PALADIN:
        case CLASS_DEATH_KNIGHT:
            return ITEM_SUBCLASS_ARMOR_PLATE;
        case CLASS_HUNTER:
        case CLASS_SHAMAN:
            return ITEM_SUBCLASS_ARMOR_MAIL;
        case CLASS_ROGUE:
        case CLASS_DRUID:
        case CLASS_MONK:
        case CLASS_DEMON_HUNTER:
            return ITEM_SUBCLASS_ARMOR_LEATHER;
        case CLASS_PRIEST:
        case CLASS_MAGE:
        case CLASS_WARLOCK:
            return ITEM_SUBCLASS_ARMOR_CLOTH;
        case CLASS_EVOKER:
            return ITEM_SUBCLASS_ARMOR_MAIL;
        default:
            return ITEM_SUBCLASS_ARMOR_CLOTH;
    }
}

namespace {

// Classic 1.60 enhancement shaman: attack power is level * 2 + Strength * 2 (no Agility: VMaNGOS / Player::UpdateAttackPowerAndDamage),
// and the bots' build takes Mental Dexterity (aura 598: attack power from 50/100/150% of Intellect). So Strength is the main stat,
// Intellect gives attack power and the mana for Stormstrike and shocks, and Agility only crit and dodge (1% crit per 20 at 60).
// The retail rule (Agility main, Strength and Intellect wasted) made the bots wear agility leather and mail over strength mail
// (Discord 12 Nov: "shaman bots gear favor agi a bit too much").
bool IsUsefulSecondaryPrimary(uint8 cls, uint16 spec, uint32 statType)
{
    if (cls == CLASS_SHAMAN && spec == 263)
        return statType == ITEM_MOD_AGILITY || statType == ITEM_MOD_INTELLECT;
    return false;
}

// Stat-block scorer. Modern items can carry:
//   - Static primary (ITEM_MOD_STRENGTH/AGILITY/INTELLECT)
//   - "Smart" combined primaries that resolve to spec at runtime
//     (ITEM_MOD_AGI_STR_INT, AGI_STR, AGI_INT, STR_INT)
//   - Stamina, secondaries (Crit/Haste/Mastery/Versatility), Spirit, etc.
int32 ScoreStatBlock(ItemTemplate const* tpl, uint8 cls, uint16 spec)
{
    uint32 primary = PrimaryStatForBot(cls, spec);
    bool   healer  = IsHealerSpec(cls, spec);
    int32  total   = 0;

    for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
    {
        int32 stat_type = tpl->GetStatModifierBonusStat(i);
        if (stat_type < 0) continue;

        bool primary_match = false;
        if (uint32(stat_type) == primary)
            primary_match = true;
        else if (stat_type == ITEM_MOD_AGI_STR_INT)
            primary_match = true;  // game resolves to bot's spec
        else if (stat_type == ITEM_MOD_AGI_STR)
            primary_match = (primary == ITEM_MOD_AGILITY || primary == ITEM_MOD_STRENGTH);
        else if (stat_type == ITEM_MOD_AGI_INT)
            primary_match = (primary == ITEM_MOD_AGILITY || primary == ITEM_MOD_INTELLECT);
        else if (stat_type == ITEM_MOD_STR_INT)
            primary_match = (primary == ITEM_MOD_STRENGTH || primary == ITEM_MOD_INTELLECT);

        if (primary_match) { total += 500; continue; }

        // A second primary the spec still turns into damage (Classic 1.60 attack power, Player::UpdateAttackPowerAndDamage).
        if (IsUsefulSecondaryPrimary(cls, spec, uint32(stat_type))) { total += 250; continue; }

        // Wrong fixed primary - wasted on this bot.
        if (stat_type == ITEM_MOD_STRENGTH ||
            stat_type == ITEM_MOD_AGILITY  ||
            stat_type == ITEM_MOD_INTELLECT)
        {
            total -= 300;
            continue;
        }

        if (stat_type == ITEM_MOD_STAMINA) { total += 60; continue; }

        // Secondaries (rating). All flavors of crit/haste/mastery/vers
        // count as the same modern stat for scoring.
        if (stat_type == ITEM_MOD_CRIT_RATING       ||
            stat_type == ITEM_MOD_CRIT_MELEE_RATING ||
            stat_type == ITEM_MOD_CRIT_RANGED_RATING||
            stat_type == ITEM_MOD_CRIT_SPELL_RATING ||
            stat_type == ITEM_MOD_HASTE_RATING      ||
            stat_type == ITEM_MOD_HASTE_MELEE_RATING||
            stat_type == ITEM_MOD_HASTE_RANGED_RATING ||
            stat_type == ITEM_MOD_HASTE_SPELL_RATING ||
            stat_type == ITEM_MOD_MASTERY_RATING    ||
            stat_type == ITEM_MOD_VERSATILITY)
        {
            total += 40;
            continue;
        }

        if (stat_type == ITEM_MOD_SPIRIT)
        {
            total += healer ? 50 : -25;
            continue;
        }

        // Tank stats. Useful only for tank specs.
        if (stat_type == ITEM_MOD_DEFENSE_SKILL_RATING ||
            stat_type == ITEM_MOD_DODGE_RATING         ||
            stat_type == ITEM_MOD_PARRY_RATING         ||
            stat_type == ITEM_MOD_BLOCK_RATING)
        {
            bool tank_friendly = (cls == CLASS_WARRIOR && spec == 73)
                              || (cls == CLASS_PALADIN && spec == 66)
                              || (cls == CLASS_DEATH_KNIGHT && spec == 250)
                              || (cls == CLASS_DRUID && spec == 104)
                              || (cls == CLASS_MONK && spec == 268)
                              || (cls == CLASS_DEMON_HUNTER && spec == 581);
            total += tank_friendly ? 35 : 0;
            continue;
        }

        // Anything else is small / neutral.
    }
    return total;
}

} // anonymous

uint32 PrimaryStatForBot(uint8 cls, uint16 spec)
{
    switch (cls)
    {
        case CLASS_WARRIOR:
        case CLASS_DEATH_KNIGHT:
            return ITEM_MOD_STRENGTH;

        case CLASS_PALADIN:
            return spec == 65 ? ITEM_MOD_INTELLECT : ITEM_MOD_STRENGTH;  // Holy

        case CLASS_HUNTER:
        case CLASS_ROGUE:
        case CLASS_DEMON_HUNTER:
            return ITEM_MOD_AGILITY;

        case CLASS_DRUID:
            return (spec == 102 || spec == 105) ? ITEM_MOD_INTELLECT : ITEM_MOD_AGILITY;  // Bal/Resto vs Feral/Guardian

        case CLASS_MONK:
            return spec == 270 ? ITEM_MOD_INTELLECT : ITEM_MOD_AGILITY;  // MW vs BrM/WW

        case CLASS_SHAMAN:
            return spec == 263 ? ITEM_MOD_STRENGTH : ITEM_MOD_INTELLECT;  // Enh (Classic: Strength gives the attack power) vs Ele/Resto

        case CLASS_PRIEST:
        case CLASS_MAGE:
        case CLASS_WARLOCK:
        case CLASS_EVOKER:
            return ITEM_MOD_INTELLECT;

        default:
            return ITEM_MOD_STRENGTH;
    }
}

bool IsHealerSpec(uint8 cls, uint16 spec)
{
    if (cls == CLASS_PALADIN && spec == 65)  return true;
    if (cls == CLASS_PRIEST && (spec == 256 || spec == 257)) return true;
    if (cls == CLASS_DRUID && spec == 105)   return true;
    if (cls == CLASS_SHAMAN && spec == 264)  return true;
    if (cls == CLASS_MONK && spec == 270)    return true;
    if (cls == CLASS_EVOKER && spec == 1468) return true;
    return false;
}

int32 ScoreItemForClass(ItemTemplate const* tpl,
                        uint8 cls, uint16 spec, uint8 level)
{
    if (!tpl) return -1000;

    int32 score = 0;
    uint32 cls_mask = (1u << (cls - 1));

    // AllowableClass: 0 / -1 = anyone, otherwise must include bot's class.
    int32 allowable = tpl->GetAllowableClass();
    if (allowable && allowable != -1 && !(uint32(allowable) & cls_mask))
        return -1000;

    // Armor type fit
    if (tpl->GetClass() == ITEM_CLASS_ARMOR)
    {
        ItemSubclassArmor sub = ItemSubclassArmor(tpl->GetSubClass());
        ItemSubclassArmor preferred = PreferredArmorForClass(cls);
        if (sub == preferred)               score += 200;
        else if (sub == ITEM_SUBCLASS_ARMOR_MISCELLANEOUS) score += 50;
        else if (sub == ITEM_SUBCLASS_ARMOR_COSMETIC)      score += 0;
        else                                 score -= 100;
    }
    else if (tpl->GetClass() == ITEM_CLASS_WEAPON)
    {
        uint8 inv = tpl->GetInventoryType();
        switch (cls)
        {
            case CLASS_WARRIOR: case CLASS_PALADIN: case CLASS_DEATH_KNIGHT:
                if (inv == INVTYPE_2HWEAPON) score += 150;
                else if (inv == INVTYPE_WEAPON || inv == INVTYPE_WEAPONMAINHAND) score += 120;
                else if (inv == INVTYPE_SHIELD) score += 100;
                break;
            case CLASS_HUNTER:
                if (inv == INVTYPE_RANGED || inv == INVTYPE_RANGEDRIGHT) score += 200;
                else if (inv == INVTYPE_2HWEAPON || inv == INVTYPE_WEAPON) score += 80;
                break;
            case CLASS_ROGUE: case CLASS_DEMON_HUNTER:
                if (inv == INVTYPE_WEAPON || inv == INVTYPE_WEAPONMAINHAND
                 || inv == INVTYPE_WEAPONOFFHAND) score += 150;
                break;
            case CLASS_DRUID: case CLASS_MONK:
                if (inv == INVTYPE_2HWEAPON || inv == INVTYPE_WEAPON) score += 130;
                break;
            case CLASS_PRIEST: case CLASS_MAGE: case CLASS_WARLOCK:
                if (inv == INVTYPE_2HWEAPON) score += 150;
                else if (inv == INVTYPE_WEAPONMAINHAND
                      || inv == INVTYPE_WEAPON) score += 100;
                else if (inv == INVTYPE_HOLDABLE) score += 100;
                else if (inv == INVTYPE_RANGEDRIGHT) score += 50;
                break;
            case CLASS_SHAMAN:
                if (inv == INVTYPE_2HWEAPON) score += 130;
                else if (inv == INVTYPE_WEAPONMAINHAND
                      || inv == INVTYPE_WEAPONOFFHAND
                      || inv == INVTYPE_WEAPON) score += 110;
                else if (inv == INVTYPE_SHIELD) score += 90;
                break;
            case CLASS_EVOKER:
                if (inv == INVTYPE_2HWEAPON) score += 130;
                else if (inv == INVTYPE_WEAPONMAINHAND
                      || inv == INVTYPE_WEAPON) score += 100;
                break;
        }
    }

    // Stat block: primary stat match dominates.
    score += ScoreStatBlock(tpl, cls, spec);

    // Tie-break: ilvl, then quality. Use the LEVEL-SCALED effective ilvl (what
    // the bot actually wears) not the static base — a base-120 piece that the
    // 12.0 scaling curve collapses to effective ilvl 5 at this level must not
    // out-score a static green that wears at its full ~20 (the under-gearing
    // root: bots wore curve-collapsed gear). Falls back to base for non-scaling
    // items. Callers (generator, quest-reward pickers) all pass the bot's real
    // level, so effective-at-level is the correct comparison metric.
    score += EffectiveItemLevelForLevel(tpl, level);
    score += int32(tpl->GetQuality()) * 5;

    return score;
}

// ---- Classic raid-ready scoring ------------------------------------------------------------------------------------------

RaidRole RaidRoleForBot(uint8 cls, uint16 spec)
{
    if ((cls == CLASS_WARRIOR && spec == 73) || (cls == CLASS_PALADIN && spec == 66) || (cls == CLASS_DRUID && spec == 104) ||
        (cls == CLASS_DEATH_KNIGHT && spec == 250) || (cls == CLASS_MONK && spec == 268) || (cls == CLASS_DEMON_HUNTER && spec == 581))
        return RaidRole::Tank;
    if (IsHealerSpec(cls, spec))
        return RaidRole::Healer;
    switch (cls)
    {
        case CLASS_HUNTER:
            return RaidRole::Hunter;
        case CLASS_MAGE:
        case CLASS_WARLOCK:
        case CLASS_EVOKER:
            return RaidRole::Caster;
        case CLASS_PRIEST:
            return RaidRole::Caster;                                    // the non-healer priest spec is Shadow
        case CLASS_SHAMAN:
            return spec == 263 ? RaidRole::Melee : RaidRole::Caster;    // Enhancement / Elemental
        case CLASS_DRUID:
            return (spec == 102) ? RaidRole::Caster : RaidRole::Melee;  // Balance / Feral
        default:
            return RaidRole::Melee;
    }
}

char const* RaidRoleName(RaidRole role)
{
    switch (role)
    {
        case RaidRole::Tank:   return "tank";
        case RaidRole::Healer: return "healer";
        case RaidRole::Caster: return "caster";
        case RaidRole::Melee:  return "melee";
        case RaidRole::Hunter: return "hunter";
    }
    return "?";
}

bool IsPvpItem(ItemTemplate const* tpl)
{
    if (!tpl)
        return true;
    if (tpl->ExtendedData->RequiredPVPRank > 0 || tpl->HasFlag(ITEM_FLAG3_PVP_TOURNAMENT_GEAR))
        return true;
    // Battleground reputation rewards: Frostwolf Clan, Stormpike Guard, League of Arathor, The Defilers, Warsong Outriders,
    // Silverwing Sentinels. A bot that never earned the reputation is refused by CanUseItem anyway; this keeps them out
    // of the pool for a bot that somehow has it.
    switch (tpl->GetRequiredReputationFaction())
    {
        case 729: case 730: case 509: case 510: case 889: case 890:
            return true;
        default:
            break;
    }
    for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
    {
        int32 const t = tpl->GetStatModifierBonusStat(i);
        if (t == ITEM_MOD_PVP_POWER || t == ITEM_MOD_RESILIENCE_RATING)
            return true;
    }
    return false;
}

namespace {

struct RaidWeights
{
    float sta = 0, str = 0, agi = 0, intel = 0, spi = 0, armor = 0, ap = 0, rap = 0;
    float sp = 0, heal = 0, dmg = 0, school_dmg = 0, phys_dmg = 0;
    float hit = 0, crit = 0, haste = 0, mp5 = 0, mana = 0, health = 0;
    float defense = 0, dodge = 0, parry = 0, block = 0, block_value = 0;
    float dps = 0, ranged_dps = 0;
};

RaidWeights WeightsFor(RaidRole role, uint8 cls, uint16 spec)
{
    RaidWeights w;
    switch (role)
    {
        case RaidRole::Tank:
            w.sta = 1.5f; w.str = 0.6f; w.agi = 1.0f; w.armor = 0.05f; w.ap = 0.15f; w.phys_dmg = 0.3f;
            w.hit = 0.8f; w.crit = 0.5f; w.haste = 0.2f; w.health = 0.3f;
            w.defense = 2.0f; w.dodge = 1.5f; w.parry = 1.5f; w.block = 1.0f; w.block_value = 0.8f;
            w.dps = 1.5f; w.ranged_dps = 0.5f;
            break;
        case RaidRole::Healer:
            w.sta = 0.4f; w.intel = 1.2f; w.sp = 1.0f; w.heal = 1.0f; w.mp5 = 2.0f; w.crit = 0.8f; w.haste = 0.5f; w.mana = 0.1f;
            w.spi = (cls == CLASS_PALADIN) ? 0.1f : (cls == CLASS_SHAMAN ? 0.3f : 0.6f);
            w.ranged_dps = (cls == CLASS_PRIEST) ? 0.5f : 0.0f;
            break;
        case RaidRole::Caster:
            w.sta = 0.4f; w.intel = 0.9f; w.spi = 0.15f; w.sp = 1.0f; w.dmg = 1.0f; w.school_dmg = 0.5f;
            w.hit = 1.3f; w.crit = 0.9f; w.haste = 0.6f; w.mp5 = 0.6f; w.mana = 0.1f;
            w.ranged_dps = 1.2f;
            break;
        case RaidRole::Melee:
            w.sta = 0.4f; w.ap = 0.5f; w.phys_dmg = 1.0f; w.hit = 1.5f; w.crit = 1.0f; w.haste = 0.8f; w.armor = 0.01f;
            w.dps = 6.0f; w.ranged_dps = 1.0f;
            switch (cls)
            {
                case CLASS_WARRIOR: w.str = 1.0f; w.agi = 0.7f; break;
                case CLASS_PALADIN: w.str = 1.0f; w.agi = 0.4f; w.intel = 0.25f; w.mana = 0.05f; break;
                case CLASS_ROGUE:   w.str = 0.4f; w.agi = 1.0f; break;
                case CLASS_DRUID:   w.str = 0.8f; w.agi = 1.0f; w.intel = 0.15f; break;
                // Classic: 2 attack power per Strength, 1.5 per Intellect with Mental Dexterity (plus mana), Agility crit only
                case CLASS_SHAMAN:  w.str = 1.0f; w.agi = 0.55f; w.intel = 0.6f; w.mana = 0.05f; break;
                default:            w.str = 0.8f; w.agi = 0.8f; break;
            }
            break;
        case RaidRole::Hunter:
            w.sta = 0.5f; w.agi = 1.0f; w.str = 0.3f; w.intel = 0.35f; w.ap = 0.5f; w.rap = 0.5f; w.phys_dmg = 0.8f;
            w.hit = 1.4f; w.crit = 1.0f; w.haste = 0.6f; w.mp5 = 0.2f; w.mana = 0.05f;
            w.dps = 1.0f; w.ranged_dps = 8.0f;
            break;
    }
    (void)spec;
    return w;
}

// Applied value of item stat `i`: what Item::GetItemStatValue gives the wearer (budget share x random property points).
float RaidStatValue(ItemTemplate const* tpl, uint32 i, uint32 ilvl)
{
    float const pct = float(tpl->GetStatPercentEditor(i));
    if (pct == 0.0f)
        return 0.0f;
    return pct * GetRandomPropertyPoints(ilvl, tpl->GetQuality(), tpl->GetInventoryType(), tpl->GetSubClass()) * 0.0001f;
}

// Domain factor of a hit / crit / haste rating: how much of the role's weight that flavour of rating carries.
// domain 0 = any, 1 = melee, 2 = ranged, 3 = spell.
float RatingDomainFactor(RaidRole role, int domain)
{
    switch (domain)
    {
        case 1:  return (role == RaidRole::Tank || role == RaidRole::Melee) ? 1.0f : (role == RaidRole::Hunter ? 0.3f : 0.0f);
        case 2:  return role == RaidRole::Hunter ? 1.0f : (role == RaidRole::Melee ? 0.15f : 0.0f);
        case 3:  return (role == RaidRole::Caster || role == RaidRole::Healer) ? 1.0f : 0.0f;
        default: return 1.0f;
    }
}

// Weight of one stat type for the role (value per applied point), -1000 for a PvP stat. `known` is cleared when the
// stat is one the scorer has no opinion on.
float RaidStatWeight(RaidWeights const& w, RaidRole role, int32 t)
{
    switch (t)
    {
        case ITEM_MOD_MANA:                    return w.mana;
        case ITEM_MOD_HEALTH:                  return w.health;
        case ITEM_MOD_AGILITY:                 return w.agi;
        case ITEM_MOD_STRENGTH:                return w.str;
        case ITEM_MOD_INTELLECT:               return w.intel;
        case ITEM_MOD_SPIRIT:                  return w.spi;
        case ITEM_MOD_STAMINA:                 return w.sta;
        case ITEM_MOD_DEFENSE_SKILL_RATING:    return w.defense;
        case ITEM_MOD_DODGE_RATING:            return w.dodge;
        case ITEM_MOD_PARRY_RATING:            return w.parry;
        case ITEM_MOD_BLOCK_RATING:            return w.block;
        case ITEM_MOD_BLOCK_VALUE:             return w.block_value;
        case ITEM_MOD_HIT_RATING:              return w.hit * RatingDomainFactor(role, 0);
        case ITEM_MOD_HIT_MELEE_RATING:        return w.hit * RatingDomainFactor(role, 1);
        case ITEM_MOD_HIT_RANGED_RATING:       return w.hit * RatingDomainFactor(role, 2);
        case ITEM_MOD_HIT_SPELL_RATING:        return w.hit * RatingDomainFactor(role, 3);
        case ITEM_MOD_CRIT_RATING:             return w.crit * RatingDomainFactor(role, 0);
        case ITEM_MOD_CRIT_MELEE_RATING:       return w.crit * RatingDomainFactor(role, 1);
        case ITEM_MOD_CRIT_RANGED_RATING:      return w.crit * RatingDomainFactor(role, 2);
        case ITEM_MOD_CRIT_SPELL_RATING:       return w.crit * RatingDomainFactor(role, 3);
        case ITEM_MOD_HASTE_RATING:            return w.haste * RatingDomainFactor(role, 0);
        case ITEM_MOD_HASTE_MELEE_RATING:      return w.haste * RatingDomainFactor(role, 1);
        case ITEM_MOD_HASTE_RANGED_RATING:     return w.haste * RatingDomainFactor(role, 2);
        case ITEM_MOD_HASTE_SPELL_RATING:      return w.haste * RatingDomainFactor(role, 3);
        case ITEM_MOD_ATTACK_POWER:            return w.ap;
        case ITEM_MOD_RANGED_ATTACK_POWER:     return w.rap;
        case ITEM_MOD_SPELL_HEALING_DONE:      return w.heal;
        case ITEM_MOD_SPELL_DAMAGE_DONE:       return w.dmg;
        case ITEM_MOD_SPELL_POWER:             return w.sp;
        case ITEM_MOD_MANA_REGENERATION:       return w.mp5;
        case ITEM_MOD_EXTRA_ARMOR:             return w.armor;
        case ITEM_MOD_FIRE_RESISTANCE: case ITEM_MOD_FROST_RESISTANCE: case ITEM_MOD_HOLY_RESISTANCE:
        case ITEM_MOD_SHADOW_RESISTANCE: case ITEM_MOD_NATURE_RESISTANCE: case ITEM_MOD_ARCANE_RESISTANCE:
            return 0.1f;
        case ITEM_MOD_SPELL_PENETRATION:       return w.dmg > 0 ? 0.3f : 0.0f;
        case ITEM_MOD_PVP_POWER:
        case ITEM_MOD_RESILIENCE_RATING:       return -1000.0f;
        case ITEM_MOD_CLASSIC_PHYSICAL_DAMAGE_DONE: return w.phys_dmg;
        case ITEM_MOD_CLASSIC_PHYSICAL_DAMAGE_DONE + 1: case ITEM_MOD_CLASSIC_PHYSICAL_DAMAGE_DONE + 2:
        case ITEM_MOD_CLASSIC_PHYSICAL_DAMAGE_DONE + 3: case ITEM_MOD_CLASSIC_PHYSICAL_DAMAGE_DONE + 4:
        case ITEM_MOD_CLASSIC_PHYSICAL_DAMAGE_DONE + 5: case ITEM_MOD_CLASSIC_ARCANE_DAMAGE_DONE:
            return w.school_dmg;                // one school of the spell damage a caster casts, half a point per point
        case ITEM_MOD_CLASSIC_RESISTANCE_ALL_SCHOOLS: return 0.3f;
        case ITEM_MOD_AGI_STR_INT:             return w.agi + w.str + w.intel;
        case ITEM_MOD_AGI_STR:                 return w.agi + w.str;
        case ITEM_MOD_AGI_INT:                 return w.agi + w.intel;
        case ITEM_MOD_STR_INT:                 return w.str + w.intel;
        default:
            break;
    }
    // weapon skill bonuses (2H axes .. wands): a few points of skill are worth a lot to a melee wearer
    if (t >= ITEM_MOD_CLASSIC_TWOHANDED_AXES && t <= ITEM_MOD_CLASSIC_TWOHANDED_AXES + 15)
        return (role == RaidRole::Melee || role == RaidRole::Tank || role == RaidRole::Hunter) ? 2.0f : 0.0f;
    // penetration by school: casters
    if (t >= ITEM_MOD_CLASSIC_FIRE_PENETRATION && t <= ITEM_MOD_CLASSIC_ARCANE_PENETRATION)
        return w.dmg > 0 ? 0.3f : 0.0f;
    // attack power versus a creature type: only where the raid fights it (undead, demons, elementals, dragonkin)
    if (t >= ITEM_MOD_CLASSIC_ATTACK_POWER_VS_HUMANOID && t <= ITEM_MOD_CLASSIC_ATTACK_POWER_VS_MECHANICAL)
        return w.ap > 0 ? 0.12f : 0.0f;
    if (t >= ITEM_MOD_CLASSIC_SPELL_DAMAGE_VS_HUMANOID && t <= ITEM_MOD_CLASSIC_SPELL_DAMAGE_VS_BEAST)
        return w.dmg > 0 ? 0.15f : 0.0f;
    return 0.0f;
}

// Auras of the item's spells that are not item stats (older vanilla items; use and proc effects of trinkets). Equip
// effects count in full, use effects a quarter, proc effects a sixth. `mismatch` is set when the item carries an effect
// that only another role wants (a +healing trinket on a warrior).
float RaidItemSpellScore(ItemTemplate const* tpl, RaidWeights const& w, RaidRole role, bool& recognised, bool& mismatch)
{
    float total = 0.0f;
    for (ItemEffectEntry const* e : tpl->Effects)
    {
        if (!e || e->SpellID <= 0)
            continue;
        float scale;
        switch (e->TriggerType)
        {
            case ITEM_SPELLTRIGGER_ON_EQUIP: scale = 1.0f; break;
            case ITEM_SPELLTRIGGER_ON_USE:   scale = 0.25f; break;
            case ITEM_SPELLTRIGGER_ON_PROC:  scale = 0.17f; break;
            default: continue;
        }
        SpellInfo const* si = sSpellMgr->GetSpellInfo(uint32(e->SpellID), DIFFICULTY_NONE);
        if (!si)
            continue;
        for (SpellEffectInfo const& eff : si->GetEffects())
        {
            if (!eff.IsAura())
                continue;
            float const amount = std::abs(float(eff.CalcValueAsInt()));
            if (amount <= 0.0f)
                continue;
            float weight = 0.0f;
            float unit = 1.0f;          // percent -> stat points
            bool known = true;
            switch (eff.ApplyAuraName)
            {
                case SPELL_AURA_MOD_DAMAGE_DONE:
                    if (eff.MiscValue & SPELL_SCHOOL_MASK_NORMAL) weight = w.phys_dmg;
                    if (eff.MiscValue & SPELL_SCHOOL_MASK_MAGIC)
                        weight = std::max(weight, w.dmg * ((eff.MiscValue & SPELL_SCHOOL_MASK_MAGIC) == SPELL_SCHOOL_MASK_MAGIC ? 1.0f : 0.5f));
                    break;
                case SPELL_AURA_MOD_HEALING_DONE:
                case SPELL_AURA_MOD_HEALING:             weight = w.heal; break;
                case SPELL_AURA_MOD_ATTACK_POWER:        weight = w.ap; break;
                case SPELL_AURA_MOD_RANGED_ATTACK_POWER: weight = w.rap; break;
                case SPELL_AURA_MOD_HIT_CHANCE:          weight = w.hit * RatingDomainFactor(role, 1); unit = 10.0f; break;
                case SPELL_AURA_MOD_SPELL_HIT_CHANCE:    weight = w.hit * RatingDomainFactor(role, 3); unit = 10.0f; break;
                case SPELL_AURA_MOD_WEAPON_CRIT_PERCENT: weight = w.crit * RatingDomainFactor(role, 1); unit = 14.0f; break;
                case SPELL_AURA_MOD_SPELL_CRIT_CHANCE:   weight = w.crit * RatingDomainFactor(role, 3); unit = 14.0f; break;
                case SPELL_AURA_MOD_DODGE_PERCENT:       weight = w.dodge; unit = 12.0f; break;
                case SPELL_AURA_MOD_PARRY_PERCENT:       weight = w.parry; unit = 12.0f; break;
                case SPELL_AURA_MOD_BLOCK_PERCENT:       weight = w.block; unit = 12.0f; break;
                case SPELL_AURA_MOD_POWER_REGEN:         if (eff.MiscValue == POWER_MANA) weight = w.mp5; else known = false; break;
                case SPELL_AURA_MOD_MELEE_HASTE:         weight = w.haste * RatingDomainFactor(role, 1); unit = 10.0f; break;
                case SPELL_AURA_MOD_RESISTANCE:          weight = (eff.MiscValue & SPELL_SCHOOL_MASK_NORMAL) ? w.armor : 0.1f; break;
                case SPELL_AURA_MOD_SKILL:               weight = w.defense > 0 && eff.MiscValue == 95 /*Defense*/ ? w.defense : 0.0f; unit = 8.0f; break;
                default: known = false; break;
            }
            if (!known)
                continue;
            recognised = true;
            if (weight <= 0.0f)
            {
                if (e->TriggerType != ITEM_SPELLTRIGGER_ON_EQUIP || amount > 0.0f)
                    mismatch = true;
                continue;
            }
            total += scale * weight * unit * amount;
        }
    }
    return total;
}

bool IsArmorPieceSlotType(uint8 inv)
{
    switch (inv)
    {
        case INVTYPE_HEAD: case INVTYPE_SHOULDERS: case INVTYPE_CHEST: case INVTYPE_ROBE: case INVTYPE_WAIST:
        case INVTYPE_LEGS: case INVTYPE_FEET: case INVTYPE_WRISTS: case INVTYPE_HANDS:
            return true;
        default:
            return false;
    }
}

} // anonymous

int32 ResistPoints(ItemTemplate const* tpl, uint8 school, uint8 level)
{
    if (!tpl)
        return 0;
    int32 mod;
    switch (school)
    {
        case SPELL_SCHOOL_HOLY:   mod = ITEM_MOD_HOLY_RESISTANCE;   break;
        case SPELL_SCHOOL_FIRE:   mod = ITEM_MOD_FIRE_RESISTANCE;   break;
        case SPELL_SCHOOL_NATURE: mod = ITEM_MOD_NATURE_RESISTANCE; break;
        case SPELL_SCHOOL_FROST:  mod = ITEM_MOD_FROST_RESISTANCE;  break;
        case SPELL_SCHOOL_SHADOW: mod = ITEM_MOD_SHADOW_RESISTANCE; break;
        case SPELL_SCHOOL_ARCANE: mod = ITEM_MOD_ARCANE_RESISTANCE; break;
        default:                  return 0;
    }
    uint32 const ilvl = uint32(std::max<int32>(1, EffectiveItemLevelForLevel(tpl, level)));
    float points = 0.0f;
    for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
    {
        int32 const t = tpl->GetStatModifierBonusStat(i);
        // Resistance to all schools is the five magic schools (Player::_ApplyItemBonuses), not holy.
        if (t == mod || (t == ITEM_MOD_CLASSIC_RESISTANCE_ALL_SCHOOLS && school != SPELL_SCHOOL_HOLY))
            points += RaidStatValue(tpl, i, ilvl);
    }
    return int32(points + 0.5f);
}

int32 ScoreItemRaid(ItemTemplate const* tpl, uint8 cls, uint16 spec, uint8 level, uint8 slot)
{
    if (!tpl)
        return -100000;
    if (IsPvpItem(tpl))
        return -100000;
    int32 const allowable = tpl->GetAllowableClass();
    if (allowable && allowable != -1 && !(uint32(allowable) & (1u << (cls - 1))))
        return -100000;

    RaidRole const role = RaidRoleForBot(cls, spec);
    RaidWeights const w = WeightsFor(role, cls, spec);
    uint32 const ilvl = uint32(std::max<int32>(1, EffectiveItemLevelForLevel(tpl, level)));
    uint8 const inv = tpl->GetInventoryType();

    float total = 0.0f;
    for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
    {
        int32 const t = tpl->GetStatModifierBonusStat(i);
        if (t < 0)
            continue;
        float const weight = RaidStatWeight(w, role, t);
        if (weight < -100.0f)
            return -100000;
        if (weight != 0.0f)
            total += weight * RaidStatValue(tpl, i, ilvl);
    }

    // Armor of the piece (tanks live on it) and weapon damage per second.
    if (w.armor > 0.0f && tpl->GetClass() == ITEM_CLASS_ARMOR)
        total += w.armor * float(tpl->GetArmor(ilvl));
    if (tpl->GetClass() == ITEM_CLASS_WEAPON)
    {
        float const dps = tpl->GetDPS(ilvl);
        if (slot == EQUIPMENT_SLOT_RANGED)
            total += w.ranged_dps * dps;
        else if (slot == EQUIPMENT_SLOT_OFFHAND)
            total += w.dps * dps * 0.5f;
        else
            total += w.dps * dps;
    }

    // Item spells (equip / use / proc auras) that are not stats.
    bool recognised = false, mismatch = false;
    float const spells = tpl->Effects.empty() ? 0.0f : RaidItemSpellScore(tpl, w, role, recognised, mismatch);
    total += spells;
    if (inv == INVTYPE_TRINKET)
    {
        // A trinket's point is its effect, which no stat weight sees: unknown effects get a small credit, an effect only
        // another role wants a penalty (a healer trinket on a warrior).
        bool const has_effect = !tpl->Effects.empty();
        if (has_effect && !recognised)
            total += 20.0f;
        else if (recognised && spells > 0.0f)
            total += 20.0f;
        else if (mismatch)
            total -= 60.0f;
    }

    // Armor type: a piece one tier under the class's own (mail on a warrior, leather on a shaman) loses the armor and
    // the class bonuses; about a fifth of what it is worth.
    if (tpl->GetClass() == ITEM_CLASS_ARMOR && IsArmorPieceSlotType(inv))
    {
        ItemSubclassArmor const preferred = PreferredArmorForClass(cls);
        uint32 const sub = tpl->GetSubClass();
        if (sub >= ITEM_SUBCLASS_ARMOR_CLOTH && sub <= ITEM_SUBCLASS_ARMOR_PLATE && sub < uint32(preferred))
            total *= (role == RaidRole::Tank) ? 0.6f : 0.8f;
    }

    // Weapon style. A two-hander carries the budget of both hands and leaves no offhand: right for the classes that
    // fight with one (arms, retribution, enhancement, feral and bear, hunter melee), wrong for the ones that
    // carry a shield or a second weapon (rogue, fury, protection, holy paladin).
    if (slot == EQUIPMENT_SLOT_MAINHAND && tpl->GetClass() == ITEM_CLASS_WEAPON)
    {
        bool const two_hand = (inv == INVTYPE_2HWEAPON);
        bool const two_hand_style =
            (cls == CLASS_WARRIOR && spec != 73 && spec != 72) || (cls == CLASS_PALADIN && spec == 70) || (cls == CLASS_SHAMAN && spec == 263) ||
            (cls == CLASS_DRUID && (spec == 103 || spec == 104)) || cls == CLASS_HUNTER;
        bool const one_hand_style =
            cls == CLASS_ROGUE || (cls == CLASS_WARRIOR && (spec == 73 || spec == 72)) || (cls == CLASS_PALADIN && (spec == 65 || spec == 66));
        if (two_hand && one_hand_style)
            total *= 0.5f;
        else if (!two_hand && two_hand_style)
            total *= 0.55f;
        // Backstab, Ambush and Hemorrhage need a dagger in the main hand.
        if (cls == CLASS_ROGUE && tpl->GetSubClass() == ITEM_SUBCLASS_WEAPON_DAGGER)
            total *= 1.15f;
    }

    // Tie-break for items with no weighted stat at all: item level and quality, so an empty slot still prefers the best.
    total += 0.1f * float(ilvl) + 0.5f * float(tpl->GetQuality());
    return int32(total * 10.0f);
}

int32 ScoreItemForBot(ItemTemplate const* tpl, Player* bot)
{
    if (!bot) return -1000;
    uint16 spec = uint16(::Playerbot::Classic::EffectiveSpecId(bot));
    return ScoreItemForClass(tpl, bot->GetClass(), spec, bot->GetLevel());
}

} // namespace Playerbot::Gear
