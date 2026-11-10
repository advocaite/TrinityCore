-- Touch of the Grave (Undead racial 1260189): the dummy proc aura now casts the drain 1260198 for $s1% of maximum health
DELETE FROM `spell_script_names` WHERE `spell_id` = 1260189 AND `ScriptName` = 'classic_spell_undead_touch_of_the_grave';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(1260189, 'classic_spell_undead_touch_of_the_grave');
