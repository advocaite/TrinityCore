-- Client 1.60.1.70707 added a column to SpellDuration.db2 (DurationPerLevel, between Duration and MaxDuration; layout hash 0x4FDF0951).
-- The server reads the 70707 layout; the hotfix table gets the same column. Applied once by the database updater.
ALTER TABLE `spell_duration` ADD COLUMN `DurationPerLevel` int NOT NULL DEFAULT 0 AFTER `Duration`;
