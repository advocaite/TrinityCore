-- Windsong Standing Stones (Zephras, Meddlesome Mages 94411): High Order Apprentices (257521) have the sniffed faction 3673,
-- whose FactionTemplate enemy group includes monsters, so they attacked the Wind Sprites (252481, faction 634, monster) and
-- the two groups killed each other with no player around. The sprites are only scenery for the quest, so they no longer
-- take on NPCs; the apprentices stay attackable by pets and guards as quest targets.
UPDATE `creature_template` SET `unit_flags` = `unit_flags` | 0x200 WHERE `entry` = 252481; -- UNIT_FLAG_IMMUNE_TO_NPC

-- Fly Away! (1253200, invisibility + fear, 5-8 sec) ran on every reset, so the sprites kept vanishing. Their only text is
-- "The tiny sprite squeaks fearfully and vanishes!", so it is their escape when attacked (INFERRED: the event is not sniffed).
DELETE FROM `smart_scripts` WHERE `entryorguid` = 252481 AND `source_type` = 0 AND `id` IN (0, 1);
INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`, `event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `action_type`, `action_param1`, `action_param2`, `target_type`, `comment`) VALUES
(252481, 0, 0, 1, 4, 0, 100, 0, 0, 0, 0, 0, 1, 0, 0, 1, 'Wind Sprite - On Aggro - Say Line 0'),
(252481, 0, 1, 0, 61, 0, 100, 0, 0, 0, 0, 0, 11, 1253200, 0, 1, 'Wind Sprite - Linked - Cast Fly Away! on Self');

-- Cirrusfly Queen (251404, The Cirrusfly Queen 92463): two sniffed spawns 11 yards apart, so two queens stood side by side.
-- 21100464 walks her waypoint path; 21100465 is a second sighting of the same queen (random movement, no path, no pool).
DELETE FROM `creature_addon` WHERE `guid` = 21100465;
DELETE FROM `creature` WHERE `guid` = 21100465 AND `id` = 251404;
