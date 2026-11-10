-- Classic 1.60 (WoW Forever) warrior Victory Rush 402927: the damage is 1 plus 15% of attack power (tooltip ${1+$AP*$m3/100}); the
-- client spell only carries the 1, so it hit for 1 or 2. C++: Custom/Classic/world/talents/classic_talents_warrior_rogue_hunter.cpp.
-- Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_warr_victory_rush';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES (402927, 'classic_spell_warr_victory_rush');
