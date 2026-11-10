-- UBRS, Hall of Binding: the Blackrock Altar (175706, summoning ritual, three grouped users) casts 16533 "Emberseer Start",
-- which sends game event 4884. classic_instance_blackrock_spire now handles that event (ProcessEvent): it frees the
-- Blackhand Incarcerators, closes the doors and tells Pyroguard Emberseer to start growing by SetData(1, 1).
-- This row is that "on data set 1 1" hook: SmartAI phase 3 is the growth phase the converted VMaNGOS rows (ids 2-10, phase
-- mask 4093) run in: 5 s Growing + Freeze Anim, 35 s and 65 s texts, Full Strength, then phase 4 (yell, faction, combat).
-- Until now nothing moved him out of phase 2 (caged), so the altar did nothing.
DELETE FROM `smart_scripts` WHERE `entryorguid` = 9816 AND `source_type` = 0 AND `id` = 16;
INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`, `Difficulties`, `event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `event_param_string`, `action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`, `action_param7`, `action_param_string`, `target_type`, `target_param1`, `target_param2`, `target_param3`, `target_param4`, `target_param_string`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
(9816, 0, 16, 0, '', 38, 0, 100, 0, 1, 1, 0, 0, 0, '', 22, 3, 0, 0, 0, 0, 0, 0, '', 1, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Pyroguard Emberseer - On Data Set 1 1 (Emberseer Start, event 4884) - Set Phase 3');
