-- Molten Core: the Flamewaker Priests (11662, Sulfuron Harbinger's four adds) never healed. Their Dark Mending (19775, 27,750-32,250)
-- row from the VMaNGOS conversion (2026_09_28_15_world_vanilla_smartai.sql) fires on a friendly creature between 0 and 1 percent
-- health within 30 yards, which never happens. Vanilla: the most hurt ally, every 5-10 s, no threshold. SmartAI event 74 with the
-- action-invoker target (7) picks the lowest-health friendly within the event's radius, as the Flamewaker Healer's row does.
-- Shadow Shock (id 1), Immolate (id 2) and Dark Strike (id 3) rows stay as they are. Safe to run again.
DELETE FROM `smart_scripts` WHERE `entryorguid` = 11662 AND `source_type` = 0 AND `id` = 0;
INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`, `Difficulties`, `event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `event_param_string`, `action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`, `action_param7`, `action_param_string`, `target_type`, `target_param1`, `target_param2`, `target_param3`, `target_param4`, `target_param_string`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
(11662, 0, 0, 0, '', 74, 0, 100, 0, 0, 99, 5000, 10000, 30, '', 11, 19775, 0, 0, 0, 0, 0, 0, '', 7, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Flamewaker Priest - Friendly Below 100% HP - Cast ''Dark Mending'' (VMaNGOS spell list 116620)');
