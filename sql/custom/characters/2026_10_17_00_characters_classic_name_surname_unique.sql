-- Classic 1.60: the same name or the same surname may be used by several characters, only the combination of both has to be unique.
-- Replaces the unique key on `name` with a unique key on (`name`, `surname`). Safe to run again.
SET @sql := IF((SELECT COUNT(*) FROM information_schema.STATISTICS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'characters' AND INDEX_NAME = 'idx_name') > 0, 'ALTER TABLE `characters` DROP INDEX `idx_name`', 'DO 0');
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @sql := IF((SELECT COUNT(*) FROM information_schema.STATISTICS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'characters' AND INDEX_NAME = 'idx_name_surname') = 0, 'ALTER TABLE `characters` ADD UNIQUE KEY `idx_name_surname` (`name`, `surname`)', 'DO 0');
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;
