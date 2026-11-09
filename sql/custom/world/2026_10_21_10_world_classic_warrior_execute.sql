-- Classic 1.60: Execute (5308 and the four higher ranks). The client spell is a dummy effect, the damage spell 20647 was never cast.
-- C++: Custom/Classic/world/talents/classic_talents_warrior_rogue_hunter.cpp (classic_spell_warr_execute). The retail script bound to
-- the first rank (written for the retail trigger-spell layout) never matched the dummy effect. Safe to run again.
DELETE FROM `spell_script_names` WHERE (`spell_id` = -5308 AND `ScriptName` = 'spell_warr_execute')
    OR (`spell_id` = 5308 AND `ScriptName` = 'spell_warr_execute')
    OR (`spell_id` IN (5308, 20658, 20660, 20661, 20662) AND `ScriptName` = 'classic_spell_warr_execute');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(5308, 'classic_spell_warr_execute'),
(20658, 'classic_spell_warr_execute'),
(20660, 'classic_spell_warr_execute'),
(20661, 'classic_spell_warr_execute'),
(20662, 'classic_spell_warr_execute');
