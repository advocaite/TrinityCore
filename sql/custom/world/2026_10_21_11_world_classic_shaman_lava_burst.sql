-- Classic 1.60: Lava Burst (408490 and the two trainer ranks) always critically strikes a target carrying the shaman's Flame Shock.
-- C++: Custom/Classic/world/talents/classic_talents_mage_shaman.cpp (classic_spell_sha_lava_burst). Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_sha_lava_burst';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(408490, 'classic_spell_sha_lava_burst'),
(1238299, 'classic_spell_sha_lava_burst'),
(1238300, 'classic_spell_sha_lava_burst');
