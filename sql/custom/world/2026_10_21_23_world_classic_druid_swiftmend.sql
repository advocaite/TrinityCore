-- Classic 1.60: Swiftmend (18562). The client spell is a heal of 1 point that consumed nothing. C++: Custom/Classic/world/talents/
-- classic_talents_warlock_druid.cpp (classic_spell_dru_swiftmend). Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` = 18562 AND `ScriptName` = 'classic_spell_dru_swiftmend';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(18562, 'classic_spell_dru_swiftmend');
