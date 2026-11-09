-- Classic 1.60 (client 70790): Improved Whirlwind (1310315, Fury talent) "increases the damage it deals to the closest enemy by $s2%" is a
-- dummy effect that no handler read. The Whirlwind script adds it. C++: Custom/Classic/world/talents/classic_talents_warrior_rogue_hunter.cpp.
-- Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_warr_improved_whirlwind_closest';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(1680, 'classic_spell_warr_improved_whirlwind_closest');
