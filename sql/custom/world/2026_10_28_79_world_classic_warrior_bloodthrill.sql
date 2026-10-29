-- Classic 1.60: Bloodthrill (warrior talent 1289682) gives an Overpower activation at the rank's chance, from main hand attacks on a
-- target with the warrior's Rend (classic_talents_warrior_rogue_hunter.cpp). Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` = 1289682 AND `ScriptName` = 'classic_spell_warr_bloodthrill';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(1289682, 'classic_spell_warr_bloodthrill');
