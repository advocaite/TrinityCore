-- Classic 1.60: creature_classic_stats now holds vanilla stat MULTIPLIERS (2026_10_11_11); the earlier absolute-value table of the same name
-- (health_min / health_max / armor...; created by 2026_10_11_00_world_classic_creature_vanilla) is dropped here so 2026_10_11_11 can create the
-- current one. A table already in the current form is left alone.
SET @old := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'creature_classic_stats' AND COLUMN_NAME = 'health_min');
SET @sql := IF(@old > 0, 'DROP TABLE `creature_classic_stats`', 'DO 0');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;
