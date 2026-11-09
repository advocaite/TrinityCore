-- Classic 1.60 (vanilla): the absorb shields scale with spell power again. VMaNGOS gives them their share in code, not in the table
-- (SpellAuras.cpp HandleSchoolAbsorb): Power Word: Shield takes 10% of healing power, Fire Ward and Frost Ward 10% of spell damage, times the
-- level penalty of the rank (1 - (20 - level) x 3.75% below level 20). The table rows taken across by 2026_10_10_21 carry 0, and since the
-- absorb path reads the table (2026_10_17 coefficient fix) a 0 there means no scaling at all. The table value is the penalised one, so the
-- ranks below level 20 carry 0.1 x the penalty (rank 1 level 6: 0.0475, rank 2 level 12: 0.07, rank 3 level 18: 0.0925). The spells are the
-- ones VMaNGOS picks by family and class mask (priest bit 0, mage bits 3 and 8); Shadow Ward has no family in the data, so it gets nothing
-- there and nothing here. Same rule as the generator (tools/classic_spell_values/gen_spell_values.py, absorb_coeff). Safe to run again.
UPDATE `classic_spell_value` SET `bonus_coeff` = 0.0475 WHERE `spell_id` = 17 AND `effect_index` = 0 AND `apply_aura` = 69;
UPDATE `classic_spell_value` SET `bonus_coeff` = 0.07 WHERE `spell_id` = 592 AND `effect_index` = 0 AND `apply_aura` = 69;
UPDATE `classic_spell_value` SET `bonus_coeff` = 0.0925 WHERE `spell_id` = 600 AND `effect_index` = 0 AND `apply_aura` = 69;
-- Power Word: Shield ranks 4-10 (3747-10901) and the priest-family copies 27607 and 27779, Fire Ward 543-10225 and 15041, Frost Ward 6143-28609 and 15044
UPDATE `classic_spell_value` SET `bonus_coeff` = 0.1 WHERE `spell_id` IN (3747, 6065, 6066, 10898, 10899, 10900, 10901, 27607, 27779, 543, 8457, 8458, 10223, 10225, 15041, 6143, 8461, 8462, 10177, 28609, 15044) AND `effect_index` = 0 AND `apply_aura` = 69;
