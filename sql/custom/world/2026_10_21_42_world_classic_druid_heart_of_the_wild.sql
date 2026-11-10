-- Classic 1.60: Heart of the Wild (17003). The client talent has the intellect aura and two dummy effects with the Bear / Dire Bear (stamina) and
-- Cat (strength) percentages; the form auras 24899 and 24900 were never cast. C++: Custom/Classic/world/talents/classic_talents_warlock_druid.cpp
-- (UpdateHeartOfTheWild from classic_talent_dru_form_talents on the form spells, classic_talent_dru_heart_of_the_wild on the talent).
-- Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` = 17003 AND `ScriptName` = 'classic_talent_dru_heart_of_the_wild';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES (17003, 'classic_talent_dru_heart_of_the_wild');
