-- Classic 1.60 Ice Lance (400640): classic_spell_mage_ice_lance multiplies the damage by the percent of effect 1 (300) against a Frozen
-- target or with a charge of Fingers of Frost (src/server/scripts/Custom/Classic/world/talents/classic_talents_mage_shaman.cpp).
-- 400640 keeps classic_spell_mage_frozen_target (Fingers of Frost charge, Shatter). Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_mage_ice_lance' OR (`spell_id` = 400640 AND `ScriptName` LIKE 'spell_mage%');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(400640, 'classic_spell_mage_ice_lance');
