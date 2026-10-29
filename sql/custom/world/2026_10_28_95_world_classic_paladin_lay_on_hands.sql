-- Classic 1.60: Lay on Hands (633, 2800, 10310) drains all of the paladin's mana (classic_talents_priest_paladin.cpp). Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` IN (633, 2800, 10310) AND `ScriptName` = 'classic_spell_pal_lay_on_hands';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(633, 'classic_spell_pal_lay_on_hands'),
(2800, 'classic_spell_pal_lay_on_hands'),
(10310, 'classic_spell_pal_lay_on_hands');
