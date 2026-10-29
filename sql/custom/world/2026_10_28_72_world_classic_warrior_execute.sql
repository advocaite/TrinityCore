-- Classic 1.60: Execute (5308, 20658, 20660, 20661, 20662) deals its damage and spends the warrior's extra rage
-- (classic_talents_warrior_rogue_hunter.cpp). Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` IN (5308, 20658, 20660, 20661, 20662) AND `ScriptName` = 'classic_spell_warr_execute';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(5308, 'classic_spell_warr_execute'),
(20658, 'classic_spell_warr_execute'),
(20660, 'classic_spell_warr_execute'),
(20661, 'classic_spell_warr_execute'),
(20662, 'classic_spell_warr_execute');
