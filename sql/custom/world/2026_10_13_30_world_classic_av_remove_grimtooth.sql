-- Classic 1.60: Alterac Valley has a level 60 hostile "Grimtooth" (entry 603) at (-1594, -410, 81.5) that vanilla never spawned (the VMaNGOS dump has the
-- template but no spawn anywhere). Removes his spawns on map 30, with the rows that hang off those spawns. Safe to run again.
-- Sorts after 2026_10_13_10 (the AV spawn groups) so a fresh database ends without him.
-- The spawn guids are read from `creature` itself (still holding the rows until the last statement): a TEMPORARY table cannot be named twice in one statement.
DELETE FROM `creature_addon` WHERE `guid` IN (SELECT `guid` FROM `creature` WHERE `map` = 30 AND `id` = 603);
DELETE FROM `creature_formations` WHERE `memberGUID` IN (SELECT `guid` FROM `creature` WHERE `map` = 30 AND `id` = 603) OR `leaderGUID` IN (SELECT `guid` FROM `creature` WHERE `map` = 30 AND `id` = 603);
DELETE FROM `creature_movement_override` WHERE `SpawnId` IN (SELECT `guid` FROM `creature` WHERE `map` = 30 AND `id` = 603);
DELETE FROM `linked_respawn` WHERE `guid` IN (SELECT `guid` FROM `creature` WHERE `map` = 30 AND `id` = 603) OR `linkedGuid` IN (SELECT `guid` FROM `creature` WHERE `map` = 30 AND `id` = 603);
DELETE FROM `game_event_creature` WHERE `guid` IN (SELECT `guid` FROM `creature` WHERE `map` = 30 AND `id` = 603);
DELETE FROM `pool_members` WHERE `type` = 0 AND `spawnId` IN (SELECT `guid` FROM `creature` WHERE `map` = 30 AND `id` = 603);
DELETE FROM `spawn_group` WHERE `spawnType` = 0 AND `spawnId` IN (SELECT `guid` FROM `creature` WHERE `map` = 30 AND `id` = 603);
DELETE FROM `creature` WHERE `map` = 30 AND `id` = 603;
