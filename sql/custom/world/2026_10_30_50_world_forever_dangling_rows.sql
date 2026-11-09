-- Rows that point at NPCs which no longer exist in the world (their templates came with the data of another server and are not shipped):
-- quest starters and quest turn-ins of such NPCs, and their spawns. A quest that could still be accepted but never handed in is the
-- worst case, so a quest that loses its turn-in NPC also loses its starters (and the other way round). Safe to run again; on a
-- world that still has those templates nothing is touched.
DROP TABLE IF EXISTS `tmp_dangling_quests`;
CREATE TABLE `tmp_dangling_quests` (`quest` INT UNSIGNED NOT NULL, PRIMARY KEY (`quest`));
INSERT IGNORE INTO `tmp_dangling_quests` (`quest`) SELECT `quest` FROM `creature_queststarter` WHERE `id` NOT IN (SELECT `entry` FROM `creature_template`);
INSERT IGNORE INTO `tmp_dangling_quests` (`quest`) SELECT `quest` FROM `creature_questender` WHERE `id` NOT IN (SELECT `entry` FROM `creature_template`);

DELETE FROM `creature_queststarter` WHERE `id` NOT IN (SELECT `entry` FROM `creature_template`);
DELETE FROM `creature_questender` WHERE `id` NOT IN (SELECT `entry` FROM `creature_template`);

-- a quest of that list with no turn-in left cannot be accepted any more; one with no starter left has no turn-in either
DELETE FROM `creature_queststarter` WHERE `quest` IN (SELECT `quest` FROM `tmp_dangling_quests`)
    AND `quest` NOT IN (SELECT `quest` FROM `creature_questender` UNION SELECT `quest` FROM `gameobject_questender`);
DELETE FROM `gameobject_queststarter` WHERE `quest` IN (SELECT `quest` FROM `tmp_dangling_quests`)
    AND `quest` NOT IN (SELECT `quest` FROM `creature_questender` UNION SELECT `quest` FROM `gameobject_questender`);
DELETE FROM `creature_questender` WHERE `quest` IN (SELECT `quest` FROM `tmp_dangling_quests`)
    AND `quest` NOT IN (SELECT `quest` FROM `creature_queststarter` UNION SELECT `quest` FROM `gameobject_queststarter`);
DELETE FROM `gameobject_questender` WHERE `quest` IN (SELECT `quest` FROM `tmp_dangling_quests`)
    AND `quest` NOT IN (SELECT `quest` FROM `creature_queststarter` UNION SELECT `quest` FROM `gameobject_queststarter`);
DROP TABLE `tmp_dangling_quests`;

-- spawns of NPCs without a template, and what hangs on those spawns
DROP TABLE IF EXISTS `tmp_dangling_spawns`;
CREATE TABLE `tmp_dangling_spawns` (`guid` BIGINT UNSIGNED NOT NULL, PRIMARY KEY (`guid`));
INSERT INTO `tmp_dangling_spawns` (`guid`) SELECT `guid` FROM `creature` WHERE `id` NOT IN (SELECT `entry` FROM `creature_template`);
DELETE FROM `creature_addon` WHERE `guid` IN (SELECT `guid` FROM `tmp_dangling_spawns`);
DELETE FROM `pool_members` WHERE `type`=0 AND `spawnId` IN (SELECT `guid` FROM `tmp_dangling_spawns`);
DELETE FROM `spawn_group` WHERE `spawnType`=0 AND `spawnId` IN (SELECT `guid` FROM `tmp_dangling_spawns`);
DELETE FROM `game_event_creature` WHERE `guid` IN (SELECT `guid` FROM `tmp_dangling_spawns`);
DELETE FROM `creature` WHERE `guid` IN (SELECT `guid` FROM `tmp_dangling_spawns`);
DROP TABLE `tmp_dangling_spawns`;
