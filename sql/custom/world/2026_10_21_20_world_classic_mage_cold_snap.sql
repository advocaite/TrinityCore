-- Classic 1.60 Cold Snap (12472): the client spell is a dummy effect; classic_spell_mage_cold_snap resets the Frost mage spells on cooldown
-- (src/server/scripts/Custom/Classic/world/talents/classic_talents_mage_shaman.cpp). Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_mage_cold_snap' OR (`spell_id` = 12472 AND `ScriptName` LIKE 'spell_mage%');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(12472, 'classic_spell_mage_cold_snap');
