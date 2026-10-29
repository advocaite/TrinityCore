-- Classic 1.60: Vindication (paladin talent 9452) lowers the attack power of the paladin's melee targets and raises the paladin's
-- (classic_talents_priest_paladin.cpp). Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` = 9452 AND `ScriptName` = 'classic_spell_pal_vindication';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(9452, 'classic_spell_pal_vindication');
