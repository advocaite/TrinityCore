-- Classic 1.60 (WoW Forever) mage talent Cold Snap (12472): finishes the cooldown of the other Frost spells
-- (src/server/scripts/Custom/Classic/world/talents/classic_talents_mage_shaman.cpp)

DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_mage_cold_snap';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(12472, 'classic_spell_mage_cold_snap');
