-- Classic 1.60: Overpower (7384 and the three higher ranks). Logs the combo points and combo cost each cast sees, to find why runs cast it
-- without a dodge. C++: Custom/Classic/world/talents/classic_talents_warrior_rogue_hunter.cpp (classic_spell_warr_overpower_probe). Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` IN (7384, 7887, 11584, 11585) AND `ScriptName` = 'classic_spell_warr_overpower_probe';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(7384, 'classic_spell_warr_overpower_probe'),
(7887, 'classic_spell_warr_overpower_probe'),
(11584, 'classic_spell_warr_overpower_probe'),
(11585, 'classic_spell_warr_overpower_probe');
