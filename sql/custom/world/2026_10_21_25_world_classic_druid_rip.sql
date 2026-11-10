-- Classic 1.60: Rip (1079, 9492, 9493, 9752, 9894, 9896) gains attack power per tick as in vMaNGOS (patch 1.12), combo points counted to 4.
-- C++: Custom/Classic/world/talents/classic_talents_warlock_druid.cpp (classic_spell_dru_rip). Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` IN (1079, 9492, 9493, 9752, 9894, 9896) AND `ScriptName` = 'classic_spell_dru_rip';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(1079, 'classic_spell_dru_rip'),
(9492, 'classic_spell_dru_rip'),
(9493, 'classic_spell_dru_rip'),
(9752, 'classic_spell_dru_rip'),
(9894, 'classic_spell_dru_rip'),
(9896, 'classic_spell_dru_rip');
