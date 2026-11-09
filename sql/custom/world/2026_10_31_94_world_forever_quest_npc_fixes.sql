-- Never Coming Back (98251, Ashenvale): the giver Va'xug Firefure (267214) was added with faction 90 (demon, hostile to every
-- player), so nobody could talk to him and he would be attacked on sight. Same friendly faction as other Forever quest givers.
UPDATE `creature_template` SET `faction` = 35 WHERE `entry` = 267214 AND `faction` = 90;

-- Melding of Influences (4642, Un'Goro): the Captured Felwood Ooze (10290, summoned by the Encased Corrupt Ooze, spell 16031)
-- only started to follow the Primal Ooze when something set data 0 1 on it, and nothing did; when it arrived it just vanished.
-- Nothing ever made the Gargantuan Ooze (9621), the only creature that drops the Merged Ooze Sample (12291), so the quest could
-- not be finished. Now: on summon it walks to the closest Primal Ooze (6557) within 40 yards; when it reaches it, it casts
-- Merging Oozes (16032) for the visual, the Primal Ooze goes away and a Gargantuan Ooze appears in its place for 5 minutes.
DELETE FROM `smart_scripts` WHERE `entryorguid` = 10290 AND `source_type` = 0;
INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`, `Difficulties`, `event_type`, `event_phase_mask`, `event_chance`, `event_flags`,
    `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `event_param_string`, `action_type`, `action_param1`, `action_param2`,
    `action_param3`, `action_param4`, `action_param5`, `action_param6`, `action_param7`, `action_param_string`, `target_type`, `target_param1`, `target_param2`,
    `target_param3`, `target_param4`, `target_param_string`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
(10290, 0, 0, 0, '', 54, 0, 100, 0, 0, 0, 0, 0, 0, '', 29, 0, 0, 6557, 0, 1, 0, 0, '', 9, 6557, 0, 40, 0, '', 0, 0, 0, 0, 'Captured Felwood Ooze - On Just Summoned - Follow Closest Primal Ooze'),
(10290, 0, 1, 0, '', 38, 0, 100, 0, 0, 1, 0, 0, 0, '', 29, 0, 0, 6557, 0, 1, 0, 0, '', 9, 6557, 0, 40, 0, '', 0, 0, 0, 0, 'Captured Felwood Ooze - On Data Set 0 1 - Follow Closest Primal Ooze'),
(10290, 0, 2, 3, '', 65, 0, 100, 0, 0, 0, 0, 0, 0, '', 11, 16032, 2, 0, 0, 0, 0, 0, '', 1, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Captured Felwood Ooze - On Follow Complete - Cast Merging Oozes'),
(10290, 0, 3, 4, '', 61, 0, 100, 0, 0, 0, 0, 0, 0, '', 12, 9621, 1, 300000, 0, 0, 0, 0, '', 1, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Captured Felwood Ooze - On Link - Summon Gargantuan Ooze'),
(10290, 0, 4, 5, '', 61, 0, 100, 0, 0, 0, 0, 0, 0, '', 41, 0, 0, 0, 0, 0, 0, 0, '', 9, 6557, 0, 10, 0, '', 0, 0, 0, 0, 'Captured Felwood Ooze - On Link - Despawn Primal Ooze'),
(10290, 0, 5, 6, '', 61, 0, 100, 0, 0, 0, 0, 0, 0, '', 47, 0, 0, 0, 0, 0, 0, 0, '', 1, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Captured Felwood Ooze - On Link - Set Visibility Off'),
(10290, 0, 6, 0, '', 61, 0, 100, 0, 0, 0, 0, 0, 0, '', 41, 1000, 0, 0, 0, 0, 0, 0, '', 1, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Captured Felwood Ooze - On Link - Despawn In 1000 ms');
UPDATE `creature_template` SET `AIName` = 'SmartAI' WHERE `entry` = 10290;
