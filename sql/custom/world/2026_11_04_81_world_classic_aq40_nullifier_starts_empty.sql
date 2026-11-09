-- Temple of Ahn'Qiraj: an Obsidian Nullifier (15312) enters the fight with no mana, as in vanilla. It fills up by draining the
-- raid (row id 1) and casts Nullify (26552, row id 0) only once it is full again. Before this it pulled with full mana and opened
-- every fight with Nullify. Safe to run again.
DELETE FROM `smart_scripts` WHERE `entryorguid` = 15312 AND `source_type` = 0 AND `id` = 3;
INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`, `Difficulties`, `event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `event_param_string`, `action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`, `action_param7`, `action_param_string`, `target_type`, `target_param1`, `target_param2`, `target_param3`, `target_param4`, `target_param_string`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
(15312, 0, 3, 0, '', 4, 0, 100, 0, 0, 0, 0, 0, 0, '', 108, 0, 0, 0, 0, 0, 0, 0, '', 1, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Obsidian Nullifier - On Aggro - Set Mana 0');
