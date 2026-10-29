-- Classic 1.60: Holy Strike (678, 679, 680, 1866, 2495, 5569, 10332, 10333) deals its full Holy bonus on top of the weapon damage
-- (classic_talents_priest_paladin.cpp). Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` IN (678, 679, 680, 1866, 2495, 5569, 10332, 10333) AND `ScriptName` = 'classic_spell_pal_holy_strike';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(678, 'classic_spell_pal_holy_strike'),
(679, 'classic_spell_pal_holy_strike'),
(680, 'classic_spell_pal_holy_strike'),
(1866, 'classic_spell_pal_holy_strike'),
(2495, 'classic_spell_pal_holy_strike'),
(5569, 'classic_spell_pal_holy_strike'),
(10332, 'classic_spell_pal_holy_strike'),
(10333, 'classic_spell_pal_holy_strike');
