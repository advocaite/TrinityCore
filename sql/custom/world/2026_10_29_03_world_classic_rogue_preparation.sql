-- Classic 1.60: Preparation (rogue talent 14185) finishes the cooldowns of the rogue's other abilities
-- (classic_talents_warrior_rogue_hunter.cpp). Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` = 14185 AND `ScriptName` = 'classic_spell_rog_preparation';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(14185, 'classic_spell_rog_preparation');
