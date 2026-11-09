-- Classic 1.60 Legacy perks with no handler. Most are in the core (Better Living Through Chemistry, Conquering Hero, Second Chance, Meticulous,
-- Optimization, Personal Banking, Play Dead); the scripts below are in classic_spell_scripts.cpp. Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('classic_spell_legacy_second_home', 'classic_spell_legacy_second_home_location',
'classic_spell_legacy_boss_celebration_buff');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
-- Second Home: set the location in a rest area, return to it elsewhere; 1262044 keeps the location
(1225494, 'classic_spell_legacy_second_home'),
(1262044, 'classic_spell_legacy_second_home_location'),
-- Boss celebration buffs: Second Chance's mark 1262777 goes when the last of them is gone
(22888, 'classic_spell_legacy_boss_celebration_buff'),
(355363, 'classic_spell_legacy_boss_celebration_buff'),
(1304452, 'classic_spell_legacy_boss_celebration_buff'),
(16609, 'classic_spell_legacy_boss_celebration_buff'),
(355366, 'classic_spell_legacy_boss_celebration_buff'),
(1304594, 'classic_spell_legacy_boss_celebration_buff'),
(24425, 'classic_spell_legacy_boss_celebration_buff');
