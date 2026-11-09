-- Classic 1.60: Refocus (24531). The client spell is a dummy effect nothing handled. C++: Custom/Classic/world/talents/classic_talents_warrior_rogue_hunter.cpp
-- (classic_spell_hun_refocus), behaviour after the vMaNGOS hunter spell script (GPL-2.0). Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` = 24531 AND `ScriptName` = 'classic_spell_hun_refocus';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(24531, 'classic_spell_hun_refocus');
