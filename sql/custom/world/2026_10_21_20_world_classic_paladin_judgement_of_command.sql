-- Classic 1.60: Judgement of Command is a dummy effect that names its Holy damage spell; the script casts it (classic_spell_scripts.cpp)
-- Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_pal_judgement_of_command';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(20425, 'classic_spell_pal_judgement_of_command'),
(20961, 'classic_spell_pal_judgement_of_command'),
(20962, 'classic_spell_pal_judgement_of_command'),
(20967, 'classic_spell_pal_judgement_of_command'),
(20968, 'classic_spell_pal_judgement_of_command');
