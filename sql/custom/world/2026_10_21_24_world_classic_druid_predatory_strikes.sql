-- Classic 1.60: Predatory Strikes (16972). The client aura carries the percent on effect 0 and an attack power effect 1 without a value.
-- C++: Custom/Classic/world/talents/classic_talents_warlock_druid.cpp (UpdatePredatoryStrikes, classic_talent_dru_predatory_strikes).
-- Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` = 16972 AND `ScriptName` = 'classic_talent_dru_predatory_strikes';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(16972, 'classic_talent_dru_predatory_strikes');
