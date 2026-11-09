-- Swift Judgement (1310994, Forever paladin talent spell): "Unleash the energy of a Seal spell upon an enemy. Does not consume the
-- Seal." Its client spell is the same script effect on an enemy as Judgement (20271), which nothing handled, so it did nothing.
-- It now uses the Judgement script (classic_spell_pal_judgement): the active seal's judgement hits the target, the seal stays.
-- Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` = 1310994 AND `ScriptName` = 'classic_spell_pal_judgement';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(1310994, 'classic_spell_pal_judgement');
