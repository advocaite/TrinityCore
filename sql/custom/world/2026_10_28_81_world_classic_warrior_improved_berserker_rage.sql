-- Classic 1.60: Improved Berserker Rage (warrior talent 20500) makes Berserker Rage (18499) generate rage and, at the rank's chance,
-- remove movement impairing effects (classic_talents_warrior_rogue_hunter.cpp). Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` = 18499 AND `ScriptName` = 'classic_spell_warr_improved_berserker_rage';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(18499, 'classic_spell_warr_improved_berserker_rage');
