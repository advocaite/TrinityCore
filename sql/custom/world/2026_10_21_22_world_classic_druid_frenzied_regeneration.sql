-- Classic 1.60: Frenzied Regeneration (22842). The client aura is a periodic trigger spell effect without a trigger spell, so nothing
-- converted rage into health. C++: Custom/Classic/world/talents/classic_talents_warlock_druid.cpp (classic_spell_dru_frenzied_regeneration).
-- Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` = 22842 AND `ScriptName` = 'classic_spell_dru_frenzied_regeneration';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(22842, 'classic_spell_dru_frenzied_regeneration');
