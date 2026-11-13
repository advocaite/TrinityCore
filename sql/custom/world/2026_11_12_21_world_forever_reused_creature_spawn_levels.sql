-- Creatures Forever placed in new places at another level keep that level there (Discord 2026-11-12, Dumpins: a Rabid Longtooth
-- with 2,938 health in the Whispering Forest, a level 14 area near 17.5, 60.6).
-- Rabid Longtooth 5288 is a level 47-48 Feralas beast in vanilla. The official Forever realm also put it in the Whispering Forest
-- (Tirisfal), and the beta sniffs saw it there at level 8-9 (2026_10_01_03 and 2026_10_06_10, creature_classic_level 8-9; its 15
-- spawns there came in with 2026_10_03_00, guids 21200001-21200015). The vanilla import 2026_10_11_00 then set the entry back to
-- 47-48 for Feralas, so the Whispering Forest ones were level 47-48 too. One level per entry cannot hold both, so these spawns get
-- their own range in creature_classic_level_spawn (read by Creature::ApplyLevelScaling before the entry's range); their health,
-- armor and hits follow from that level.
-- Of all 26 vanilla entries the sniffs placed somewhere new, only one more is at another level than its vanilla range: Redrock
-- Earth Spirit 5890 (level 1 in Durotar, level 5-6 in the four Dun Morogh spawns 21200020-21200023).
-- Safe to run again.
CREATE TABLE IF NOT EXISTS `creature_classic_level_spawn` (
  `guid` bigint unsigned NOT NULL,
  `level_min` tinyint unsigned NOT NULL,
  `level_max` tinyint unsigned NOT NULL,
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Classic 1.60: level range of one creature spawn, before creature_classic_level of its entry';

DELETE FROM `creature_classic_level_spawn` WHERE `guid` BETWEEN 21200001 AND 21200015 OR `guid` BETWEEN 21200020 AND 21200023;
INSERT INTO `creature_classic_level_spawn` (`guid`,`level_min`,`level_max`)
SELECT `guid`, 8, 9 FROM `creature` WHERE `id` = 5288 AND `map` = 0 AND `guid` BETWEEN 21200001 AND 21200015;
INSERT INTO `creature_classic_level_spawn` (`guid`,`level_min`,`level_max`)
SELECT `guid`, 5, 6 FROM `creature` WHERE `id` = 5890 AND `map` = 0 AND `guid` BETWEEN 21200020 AND 21200023;
