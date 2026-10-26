-- Classic 1.60: Touch of the Grave (Forever undead racial, 1260189 and 1260201) drains its target when it procs
-- (classic_spell_scripts.cpp). The chance and the cooldown come from the client data. Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` IN (1260189, 1260201) AND `ScriptName` = 'classic_spell_undead_touch_of_the_grave';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(1260189, 'classic_spell_undead_touch_of_the_grave'),
(1260201, 'classic_spell_undead_touch_of_the_grave');
