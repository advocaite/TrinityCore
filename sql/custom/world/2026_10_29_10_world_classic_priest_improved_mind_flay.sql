-- Classic 1.60: Improved Mind Flay (priest talent 1225139) makes each Mind Flay tick deal more than the one before (Mind Flay ranks 15407,
-- 17311, 17312, 17313, 17314, 18807; classic_talents_priest_paladin.cpp). Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_pri_improved_mind_flay';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(15407, 'classic_spell_pri_improved_mind_flay'),
(17311, 'classic_spell_pri_improved_mind_flay'),
(17312, 'classic_spell_pri_improved_mind_flay'),
(17313, 'classic_spell_pri_improved_mind_flay'),
(17314, 'classic_spell_pri_improved_mind_flay'),
(18807, 'classic_spell_pri_improved_mind_flay');
