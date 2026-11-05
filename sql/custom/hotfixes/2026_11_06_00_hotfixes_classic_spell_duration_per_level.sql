-- Classic 1.60.1.70790: SpellDuration.db2 has DurationPerLevel again as its second field (layout hash 0x4FDF0951), so the
-- hotfix table needs the column the server now selects. Safe to run again.
SET @sql := IF((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'spell_duration' AND COLUMN_NAME = 'DurationPerLevel') = 0,
    'ALTER TABLE `spell_duration` ADD COLUMN `DurationPerLevel` int NOT NULL DEFAULT ''0'' AFTER `Duration`', 'DO 0');
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;
