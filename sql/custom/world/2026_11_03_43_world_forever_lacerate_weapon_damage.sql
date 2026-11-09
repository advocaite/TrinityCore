-- Forever Lacerate (all ranks): effect 1 is a dummy, "$s2% weapon damage per existing application of Lacerate on the target".
-- Script spell_dru_lacerate_classic adds that damage to the hit.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_dru_lacerate_classic';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(1328617, 'spell_dru_lacerate_classic'),
(1328616, 'spell_dru_lacerate_classic'),
(414644, 'spell_dru_lacerate_classic'),
(1235826, 'spell_dru_lacerate_classic'),
(1235827, 'spell_dru_lacerate_classic');
