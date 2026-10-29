-- Classic 1.60: Sweeping Strikes (warrior talent 12292) strikes a second nearby enemy with each of its 5 charges. Its aura is a dummy,
-- as in later versions, where spell_warr_sweeping_strikes (spell_warrior.cpp) does the extra attack with 12723. Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` = 12292 AND `ScriptName` = 'spell_warr_sweeping_strikes';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(12292, 'spell_warr_sweeping_strikes');
