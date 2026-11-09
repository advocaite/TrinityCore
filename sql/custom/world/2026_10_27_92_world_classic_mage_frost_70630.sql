-- Client 70630 Frost mage rework.
-- Frost Ward and Ice Barrier: classic_spell_mage_frost_ward_absorb (spell power part raised by Frost Warding, Frost Ward's negate chance,
-- Ice Barrier's Frigid Power). The retail spell_mage_ice_barrier never loaded here (its Chilled 205708 is not in the client).
-- Ice Barrier takes 10% of Frost spell damage like Frost Ward ("$s1+($SPFR*0.1)"): bonus_coeff 0.1 on its value rows.
-- Winter's Chill 11180: procs from Frostbolt and Frost Blast damage hits (family flag 0x20), classic_spell_mage_winters_chill rolls the
-- talent chance and keeps the talent from spending its 70630 proc charge. Cone of Cold spends a 70630 Winter's Chill and gains its
-- spell power bonus (classic_spell_mage_cone_of_cold_winters_chill). Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('classic_spell_mage_frost_ward_absorb', 'classic_spell_mage_winters_chill', 'classic_spell_mage_cone_of_cold_winters_chill')
    OR (`spell_id` IN (11426, 13031, 13032, 13033) AND `ScriptName` = 'spell_mage_ice_barrier');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(6143, 'classic_spell_mage_frost_ward_absorb'),
(8461, 'classic_spell_mage_frost_ward_absorb'),
(8462, 'classic_spell_mage_frost_ward_absorb'),
(10177, 'classic_spell_mage_frost_ward_absorb'),
(28609, 'classic_spell_mage_frost_ward_absorb'),
(11426, 'classic_spell_mage_frost_ward_absorb'),
(13031, 'classic_spell_mage_frost_ward_absorb'),
(13032, 'classic_spell_mage_frost_ward_absorb'),
(13033, 'classic_spell_mage_frost_ward_absorb'),
(11180, 'classic_spell_mage_winters_chill'),
(120, 'classic_spell_mage_cone_of_cold_winters_chill'),
(8492, 'classic_spell_mage_cone_of_cold_winters_chill'),
(10159, 'classic_spell_mage_cone_of_cold_winters_chill'),
(10160, 'classic_spell_mage_cone_of_cold_winters_chill'),
(10161, 'classic_spell_mage_cone_of_cold_winters_chill');

UPDATE `classic_spell_value` SET `bonus_coeff` = 0.1 WHERE `spell_id` IN (11426, 13031, 13032, 13033) AND `effect_index` = 0;

DELETE FROM `spell_proc` WHERE `SpellId` = 11180;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(11180, 0, 3, 0x20, 0, 0, 0, 0x10000, 0, 1, 2, 0, 0, 0, 0, 100, 0, 0); -- Frostbolt and Frost Blast damage hits; the script rolls the talent chance
