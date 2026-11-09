-- Classic 1.60: Forever creature health curve, used only with Classic.ForeverCreatureHealth = 1 (default 0 keeps the VMaNGOS curve).
-- One curve for all classes, fit to 932 sniffed MaxHealth values of official Forever beta creatures (builds 70245-70450). Against
-- the VMaNGOS curve at levels 30 / 60: warrior-class mobs +30% / +43%, paladin-class +53% / +79%, mage-class +69% / +104%. Levels
-- 60-63 keep the level 60 ratio (few high-level samples). Data source: Azeroth project, Sakhwoow/WoWForever (GPL-2). Safe to run again.
SET @sql := IF((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'creature_classic_classlevelstats' AND COLUMN_NAME = 'health_forever') = 0, 'ALTER TABLE `creature_classic_classlevelstats` ADD COLUMN `health_forever` int unsigned NOT NULL DEFAULT 0 COMMENT ''Forever sniff health curve (Classic.ForeverCreatureHealth), 0 = use health'' AFTER `health`', 'DO 0');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;
DROP TEMPORARY TABLE IF EXISTS `tmp_classic_forever_health`;
CREATE TEMPORARY TABLE `tmp_classic_forever_health` (`class` tinyint unsigned NOT NULL, `level` tinyint unsigned NOT NULL, `health` int unsigned NOT NULL, PRIMARY KEY (`class`, `level`));
INSERT INTO `tmp_classic_forever_health` (`class`, `level`, `health`) VALUES
(1, 1, 42), (1, 2, 55), (1, 3, 71), (1, 4, 86), (1, 5, 102), (1, 6, 120), (1, 7, 137), (1, 8, 156), (1, 9, 180), (1, 10, 208), (1, 11, 239),
(1, 12, 272), (1, 13, 307), (1, 14, 345), (1, 15, 385), (1, 16, 427), (1, 17, 473), (1, 18, 521), (1, 19, 572), (1, 20, 629), (1, 21, 677),
(1, 22, 731), (1, 23, 787), (1, 24, 846), (1, 25, 909), (1, 26, 975), (1, 27, 1040), (1, 28, 1109), (1, 29, 1177), (1, 30, 1242), (1, 31, 1308),
(1, 32, 1374), (1, 33, 1443), (1, 34, 1512), (1, 35, 1586), (1, 36, 1660), (1, 37, 1737), (1, 38, 1814), (1, 39, 1897), (1, 40, 1981), (1, 41, 2061),
(1, 42, 2146), (1, 43, 2231), (1, 44, 2317), (1, 45, 2402), (1, 46, 2495), (1, 47, 2587), (1, 48, 2681), (1, 49, 2794), (1, 50, 2909), (1, 51, 3040),
(1, 52, 3174), (1, 53, 3317), (1, 54, 3458), (1, 55, 3602), (1, 56, 3755), (1, 57, 3909), (1, 58, 4105), (1, 59, 4234), (1, 60, 4365), (1, 61, 4496),
(1, 62, 4629), (1, 63, 4763), (2, 1, 42), (2, 2, 55), (2, 3, 71), (2, 4, 86), (2, 5, 102), (2, 6, 120), (2, 7, 137), (2, 8, 156), (2, 9, 180),
(2, 10, 208), (2, 11, 239), (2, 12, 272), (2, 13, 307), (2, 14, 345), (2, 15, 385), (2, 16, 427), (2, 17, 473), (2, 18, 521), (2, 19, 572),
(2, 20, 629), (2, 21, 677), (2, 22, 731), (2, 23, 787), (2, 24, 846), (2, 25, 909), (2, 26, 975), (2, 27, 1040), (2, 28, 1109), (2, 29, 1177),
(2, 30, 1242), (2, 31, 1308), (2, 32, 1374), (2, 33, 1443), (2, 34, 1512), (2, 35, 1586), (2, 36, 1660), (2, 37, 1737), (2, 38, 1814), (2, 39, 1897),
(2, 40, 1981), (2, 41, 2061), (2, 42, 2146), (2, 43, 2231), (2, 44, 2317), (2, 45, 2402), (2, 46, 2495), (2, 47, 2587), (2, 48, 2681), (2, 49, 2794),
(2, 50, 2909), (2, 51, 3040), (2, 52, 3174), (2, 53, 3317), (2, 54, 3458), (2, 55, 3602), (2, 56, 3755), (2, 57, 3909), (2, 58, 4105), (2, 59, 4234),
(2, 60, 4365), (2, 61, 4496), (2, 62, 4629), (2, 63, 4763), (8, 1, 42), (8, 2, 55), (8, 3, 71), (8, 4, 86), (8, 5, 102), (8, 6, 120), (8, 7, 137),
(8, 8, 156), (8, 9, 180), (8, 10, 208), (8, 11, 239), (8, 12, 272), (8, 13, 307), (8, 14, 345), (8, 15, 385), (8, 16, 427), (8, 17, 473),
(8, 18, 521), (8, 19, 572), (8, 20, 629), (8, 21, 677), (8, 22, 731), (8, 23, 787), (8, 24, 846), (8, 25, 909), (8, 26, 975), (8, 27, 1040),
(8, 28, 1109), (8, 29, 1177), (8, 30, 1242), (8, 31, 1308), (8, 32, 1374), (8, 33, 1443), (8, 34, 1512), (8, 35, 1586), (8, 36, 1660), (8, 37, 1737),
(8, 38, 1814), (8, 39, 1897), (8, 40, 1981), (8, 41, 2061), (8, 42, 2146), (8, 43, 2231), (8, 44, 2317), (8, 45, 2402), (8, 46, 2495), (8, 47, 2587),
(8, 48, 2681), (8, 49, 2794), (8, 50, 2909), (8, 51, 3040), (8, 52, 3174), (8, 53, 3317), (8, 54, 3458), (8, 55, 3602), (8, 56, 3755), (8, 57, 3909),
(8, 58, 4105), (8, 59, 4234), (8, 60, 4365), (8, 61, 4496), (8, 62, 4629), (8, 63, 4763);
UPDATE `creature_classic_classlevelstats` s JOIN `tmp_classic_forever_health` t ON t.`class` = s.`class` AND t.`level` = s.`level` SET s.`health_forever` = t.`health`;
DROP TEMPORARY TABLE `tmp_classic_forever_health`;
