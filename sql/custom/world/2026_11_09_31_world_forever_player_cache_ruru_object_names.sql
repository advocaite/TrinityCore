-- WoW Forever: Russian names for 16 objects that showed their English name in a Russian player's game cache (Cache\WDB\ruRU,
-- build 70862, sent 2026-11-09). Each name is the Russian name other objects with the same English name already have
-- (Wooden Bench, Chair, Mailbox, Highback Chair, Meeting Stone). Only fills a missing or empty ruRU name. Safe to run again.
DROP TEMPORARY TABLE IF EXISTS `tmp_ruru_gameobject_template_locale`;
CREATE TEMPORARY TABLE `tmp_ruru_gameobject_template_locale` (`entry` INT UNSIGNED NOT NULL, `name` MEDIUMTEXT, PRIMARY KEY (`entry`)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
INSERT INTO `tmp_ruru_gameobject_template_locale` (`entry`,`name`) VALUES
(560999,'Деревянная скамья'),
(651314,'Деревянная скамья'),
(651315,'Деревянная скамья'),
(651316,'Стул'),
(651317,'Стул'),
(651318,'Стул'),
(651319,'Стул'),
(651323,'Деревянная скамья'),
(651324,'Стул'),
(651602,'Деревянная скамья'),
(651603,'Деревянная скамья'),
(651604,'Почтовый ящик'),
(651605,'Деревянная скамья'),
(651606,'Деревянная скамья'),
(696775,'Стул с высокой спинкой'),
(700632,'Камень встреч');
UPDATE `gameobject_template_locale` l JOIN `tmp_ruru_gameobject_template_locale` t ON t.`entry`=l.`entry` AND l.`locale`='ruRU' SET l.`name`=t.`name` WHERE COALESCE(l.`name`,'')='';
INSERT INTO `gameobject_template_locale` (`entry`,`locale`,`name`,`castBarCaption`,`unk1`,`VerifiedBuild`) SELECT t.`entry`,'ruRU',t.`name`,'','',70862 FROM `tmp_ruru_gameobject_template_locale` t JOIN `gameobject_template` b ON b.`entry`=t.`entry` LEFT JOIN `gameobject_template_locale` l ON l.`entry`=t.`entry` AND l.`locale`='ruRU' WHERE l.`entry` IS NULL;
DROP TEMPORARY TABLE `tmp_ruru_gameobject_template_locale`;
