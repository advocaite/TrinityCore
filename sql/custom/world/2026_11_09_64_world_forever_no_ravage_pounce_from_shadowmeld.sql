-- Blizzard hotfix 6 November 2026: "Night Elf: Shadowmelded Druids no longer have access to Ravage and Pounce."
-- Ravage (6785, 6787, 9866, 9867) and Pounce (9005, 9823, 9827) fail while the druid has Shadowmeld (20580); Prowl works as before.
-- Script: classic_spell_dru_not_from_shadowmeld in classic_spell_scripts.cpp. Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName`='classic_spell_dru_not_from_shadowmeld';
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
(6785,'classic_spell_dru_not_from_shadowmeld'),
(6787,'classic_spell_dru_not_from_shadowmeld'),
(9866,'classic_spell_dru_not_from_shadowmeld'),
(9867,'classic_spell_dru_not_from_shadowmeld'),
(9005,'classic_spell_dru_not_from_shadowmeld'),
(9823,'classic_spell_dru_not_from_shadowmeld'),
(9827,'classic_spell_dru_not_from_shadowmeld');
