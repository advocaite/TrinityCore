-- Classic 1.60: Execute damage spell 20647. Logs what the triggered damage spell sees (check, effect launch value, damage dealt), to find why the
-- dummy's cast succeeds yet no damage lands. C++: Custom/Classic/world/talents/classic_talents_warrior_rogue_hunter.cpp (classic_spell_warr_execute_damage_probe). Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` = 20647 AND `ScriptName` = 'classic_spell_warr_execute_damage_probe';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(20647, 'classic_spell_warr_execute_damage_probe');
