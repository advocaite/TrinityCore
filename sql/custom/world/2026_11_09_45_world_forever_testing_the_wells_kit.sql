-- Testing the Wells 92742 (Westfall): the Well Testing Kit 254545 failed with "invalid target" everywhere, so its two water samples
-- (254546, 254547) could not be had (Discord 2026-11-09). The kit's spell 1258773 gets a script that gives the sample next to each well.
-- Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` = 1258773 AND `ScriptName` = 'classic_spell_quest_testing_the_wells';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES (1258773, 'classic_spell_quest_testing_the_wells');
