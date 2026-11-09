-- Classic 1.60 (WoW Forever) warrior Shield Slam: the damage adds the shield block value and the dispel works half of the time (VMaNGOS, GPL-2.0).
-- C++: Custom/Classic/world/talents/classic_talents_warrior_rogue_hunter.cpp. Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_warr_shield_slam';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(23922, 'classic_spell_warr_shield_slam'),
(23923, 'classic_spell_warr_shield_slam'),
(23924, 'classic_spell_warr_shield_slam'),
(23925, 'classic_spell_warr_shield_slam');
