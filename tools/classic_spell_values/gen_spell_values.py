# Writes the vanilla (VMaNGOS 1.12) number files for sql/custom/world from the extract of the VMaNGOS dump (see vanilla_spells.py):
#   python3 -I gen_spell_values.py values > ../../sql/custom/world/2026_10_10_21_world_classic_spell_values_v2.sql
#   python3 -I gen_spell_values.py costs  > ../../sql/custom/world/2026_10_10_22_world_classic_spell_costs.sql
#   python3 -I gen_spell_values.py mana   > ../../sql/custom/world/2026_10_10_23_world_classic_base_mana.sql
# The schema comes from 2026_10_10_20_world_classic_vanilla_schema.sql. Every file is safe to run again.
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vanilla_spells as v
from gen_spell_info import KEEP_CLIENT_COST, KEEP_CLIENT_VALUE

# SpellEffects whose value is a number worth taking across
EFFECTS = (2, 8, 9, 10, 17, 30, 31, 58, 62, 67, 121)   # SCHOOL_DAMAGE, POWER_DRAIN, HEALTH_LEECH, HEAL, WEAPON_DAMAGE_NOSCHOOL, ENERGIZE,
                                                        # WEAPON_PERCENT_DAMAGE, WEAPON_DAMAGE, POWER_BURN, HEAL_MAX_HEALTH, NORMALIZED_WEAPON_DMG
AURA_EFFECTS = (6, 27)                                  # APPLY_AURA, PERSISTENT_AREA_AURA (Flamestrike's burning ground)
AURAS = (3, 8, 13, 22, 24, 29, 34, 52, 53, 54, 55, 57, 69, 71, 79, 85, 99, 107, 108, 124, 134, 135, 136, 137)
#  3 PERIODIC_DAMAGE, 8 PERIODIC_HEAL, 13 MOD_DAMAGE_DONE, 22 MOD_RESISTANCE, 24 PERIODIC_ENERGIZE, 29 MOD_STAT, 34 MOD_INCREASE_HEALTH,
# 52 MOD_CRIT_PERCENT, 53 PERIODIC_LEECH, 54 MOD_HIT_CHANCE, 55 MOD_SPELL_HIT_CHANCE, 57 MOD_SPELL_CRIT_CHANCE, 69 SCHOOL_ABSORB,
# 71 MOD_SPELL_CRIT_CHANCE_SCHOOL, 79 MOD_DAMAGE_PERCENT_DONE, 85 MOD_POWER_REGEN, 99 MOD_ATTACK_POWER, 107 ADD_FLAT_MODIFIER,
# 108 ADD_PCT_MODIFIER, 124 MOD_RANGED_ATTACK_POWER, 134 MOD_MANA_REGEN_INTERRUPT, 135 MOD_HEALING_DONE, 136 MOD_HEALING_DONE_PERCENT,
# 137 MOD_TOTAL_STAT_PERCENTAGE
# Vanilla paladin heals carry their amount in a SCRIPT_EFFECT (77); the 70291 client has them as HEAL (10). The row is stored with the
# client's effect so SpellMgr's effect check passes. Only these spells: every other script effect is not a heal.
SCRIPT_HEALS = ('Holy Light', 'Flash of Light')
SCRIPT_EFFECT = 77
CLIENT_HEAL_EFFECT = 10
# VMaNGOS does not heal with the Flash of Light rank itself: the rank's script effect casts the shared spell 19993 with the rolled amount
# (SpellEffects.cpp, "send main spell id as basepoints"), and the spell power factor is on 19993 (0.429 = 1.5 s / 3.5). The rank rows carry
# coefficient 0. Taking that 0 across would make the heal ignore healing power, since here the rank heals directly.
FLASH_OF_LIGHT_HEAL_CARRIER = 19993
# VMaNGOS gives the absorb shields their spell power share in code, not in the table (SpellAuras.cpp HandleSchoolAbsorb): Power Word: Shield
# takes 10% of healing power, Fire Ward, Frost Ward and Shadow Ward 10% of spell damage, each times the level penalty of the rank. The table
# rows carry 0, which here would mean no scaling at all, so the rows get 0.1 times the penalty (the table value is the penalised one). The
# spells are picked the way VMaNGOS picks them: family and class mask bit (SpellClassMask.h), or icon and category for Shadow Ward. The
# extract has Shadow Ward with family 0, so that one gets nothing there and nothing here.
SCHOOL_ABSORB_AURA = 69
ABSORB_SHARE = 0.1
ABSORB_SHIELDS = (
    (6, 1 << 0, None, None),                            # priest, CF_PRIEST_POWER_WORD_SHIELD
    (3, 1 << 3, None, None),                            # mage, CF_MAGE_FIRE_WARD
    (3, 1 << 8, None, None),                            # mage, CF_MAGE_FROST_WARD
    (5, None, 207, 56),                                 # warlock Shadow Ward: icon 207, category 56
)
FIRST_SCALED_SPELL_ID = 1000000                         # ids from this on are new to Forever (ClassicSpellValue.h)


def level_penalty(spell_level):
    # VMaNGOS SpellCaster::CalculateLevelPenalty: 1 - (20 - level) x 3.75% below level 20
    if spell_level <= 0 or spell_level >= 20:
        return 1.0
    return 1.0 - (20.0 - spell_level) * 0.0375


def absorb_coeff(ix, f, a, coeff):
    if a != SCHOOL_ABSORB_AURA or coeff != 0.0 or 'spellFamilyName' not in ix:
        return coeff
    family = int(f[ix['spellFamilyName']])
    flags = int(f[ix['spellFamilyFlags']])
    icon = int(f[ix['spellIconId']])
    category = int(f[ix['category']])
    for fam, bit, want_icon, want_category in ABSORB_SHIELDS:
        if family != fam:
            continue
        if bit is not None and not (flags & bit):
            continue
        if want_icon is not None and (icon != want_icon or category != want_category):
            continue
        return round(ABSORB_SHARE * level_penalty(int(f[ix['spellLevel']])), 4)
    return coeff


def esc(n):
    return n.replace("\\'", "'").replace("'", "''")[:100]


def value_rows(ix, best):
    rows = []
    carrier_coeff = -1.0
    if FLASH_OF_LIGHT_HEAL_CARRIER in best and 'effectBonusCoefficient1' in ix:
        carrier_coeff = float(best[FLASH_OF_LIGHT_HEAL_CARRIER][1][ix['effectBonusCoefficient1']])
    for e in sorted(best):
        if e >= FIRST_SCALED_SPELL_ID:
            continue
        b, f, n = best[e]
        for i in (1, 2, 3):
            if (e, i - 1) in KEEP_CLIENT_VALUE:
                continue
            ef = int(f[ix['effect%d' % i]])
            a = int(f[ix['effectApplyAuraName%d' % i]])
            bp = int(f[ix['effectBasePoints%d' % i]])
            sides = int(f[ix['effectDieSides%d' % i]])
            ppl = float(f[ix['effectRealPointsPerLevel%d' % i]])
            misc = int(f[ix['effectMiscValue%d' % i]])
            combo = float(f[ix['effectPointsPerComboPoint%d' % i]])
            coeff = float(f[ix['effectBonusCoefficient%d' % i]]) if 'effectBonusCoefficient%d' % i in ix else -1.0
            if ef == SCRIPT_EFFECT and n in SCRIPT_HEALS and bp > 0:
                if n == 'Flash of Light' and coeff == 0.0 and carrier_coeff > 0.0:
                    coeff = carrier_coeff
                rows.append((e, i - 1, CLIENT_HEAL_EFFECT, 0, bp, sides, ppl, 0, combo, coeff, esc(n)))
            elif ef in EFFECTS or (ef in AURA_EFFECTS and a in AURAS):
                if ef in AURA_EFFECTS:
                    coeff = absorb_coeff(ix, f, a, coeff)
                rows.append((e, i - 1, ef, a if ef in AURA_EFFECTS else 0, bp, sides, ppl, misc if ef in AURA_EFFECTS else 0, combo, coeff, esc(n)))
    return rows


def fl(x):
    return repr(x) if x else '0'


def gen_values(ix, best):
    rows = value_rows(ix, best)
    out = []
    out.append("-- Classic 1.60: vanilla (1.12) values of the legacy spells (VMaNGOS spell_template, latest row of build <= 5875): damage, heal, absorb, weapon damage,")
    out.append("-- energize, per-combo-point points, the spell power factor and the numbers of the main aura effects (modifiers, damage / healing / crit / hit mods, stats, attack power).")
    out.append("-- The 70291 client data differs from vanilla for part of them (Defias Evoker Flamestrike 11829: 479 where vanilla is 75 + 1d13; player Shadow Bolt rank 10 25307:")
    out.append("-- about 257 where vanilla is 481 + 1d57). Used for every caster of the legacy spell: SpellMgr::LoadClassicSpellValues, SpellInfo.cpp CalcBaseValue / CalcValue.")
    out.append("-- An aura row is used only while the client effect has the same effect, aura and misc value. Replaces 2026_10_10_01 (schema: 2026_10_10_20). Safe to run again.")
    out.append("DELETE FROM `classic_spell_value`;")
    for k in range(0, len(rows), 500):
        out.append("INSERT INTO `classic_spell_value` (`spell_id`,`effect_index`,`effect`,`apply_aura`,`base_points`,`die_sides`,`points_per_level`,`misc_value`,`points_per_combo`,`bonus_coeff`,`spell_name`) VALUES")
        out.append(",\n".join("(%d,%d,%d,%d,%d,%d,%s,%d,%s,%s,'%s')" % (r[0], r[1], r[2], r[3], r[4], r[5], fl(r[6]), r[7], fl(r[8]), repr(r[9]), r[10]) for r in rows[k:k + 500]) + ";")
    return "\n".join(out) + "\n", len(rows)


def gen_costs(ix, best):
    rows = []
    for e in sorted(best):
        if e >= FIRST_SCALED_SPELL_ID:
            continue
        b, f, n = best[e]
        cost = int(f[ix['manaCost']])
        pct = int(f[ix['manaCostPercentage']])
        if (cost or pct) and e not in KEEP_CLIENT_COST:
            rows.append((e, cost, pct, esc(n)))
    out = ["-- Classic 1.60: vanilla (1.12) mana cost of the legacy spells (VMaNGOS spell_template manaCost / manaCostPercentage, latest row of build <= 5875).",
           "-- SpellInfo::CalcPowerCost uses it instead of the client's SpellPower row; talent and aura cost modifiers still apply. Safe to run again.",
           "DELETE FROM `classic_spell_cost`;"]
    for k in range(0, len(rows), 500):
        out.append("INSERT INTO `classic_spell_cost` (`spell_id`,`mana_cost`,`mana_cost_pct`,`spell_name`) VALUES")
        out.append(",\n".join("(%d,%d,%d,'%s')" % r for r in rows[k:k + 500]) + ";")
    return "\n".join(out) + "\n", len(rows)


def gen_mana(stats):
    rows = sorted((c, l, s['basemana']) for (c, l), s in stats.items() if s.get('basemana', 0) and l <= 60)
    out = ["-- Classic 1.60 (vanilla): base mana of the class at the level from VMaNGOS 1.12 player_classlevelstats.basemana, like base health",
           "-- (2026_10_05_13). ObjectMgr::GetPlayerClassLevelInfo reads it; a class or level without a value keeps the client's game table. Safe to run again.",
           "UPDATE `player_classlevelstats` SET `basemana` = 0 WHERE `level` <= 60;"]
    for c, l, m in rows:
        out.append("UPDATE `player_classlevelstats` SET `basemana` = %d WHERE `class` = %d AND `level` = %d;" % (m, c, l))
    return "\n".join(out) + "\n", len(rows)


if __name__ == '__main__':
    what = sys.argv[1] if len(sys.argv) > 1 else 'values'
    ix, best, stats = v.load_all()
    if what == 'values':
        text, n = gen_values(ix, best)
    elif what == 'costs':
        text, n = gen_costs(ix, best)
    elif what == 'mana':
        text, n = gen_mana(stats)
    else:
        sys.exit('usage: gen_spell_values.py values|costs|mana')
    sys.stdout.write(text)
    sys.stderr.write("%s: %d rows\n" % (what, n))
