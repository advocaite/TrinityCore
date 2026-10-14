-- Classic 1.60: schema for taking more of the VMaNGOS 1.12 numbers across. No data here (the generated files that follow fill it);
-- an empty column or table changes nothing. Safe to run again.
--  * classic_spell_value.misc_value / points_per_combo / bonus_coeff: aura effects are matched on their misc value, finishers get their vanilla points per combo point
--  * classic_spell_cost: vanilla mana cost of the legacy spells (flat + percent of base mana)
--  * player_classlevelstats.basemana: vanilla base mana of the class at the level (0 = keep the client's game table value)
SET @sql := IF((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'classic_spell_value' AND COLUMN_NAME = 'misc_value') = 0,
    'ALTER TABLE `classic_spell_value` ADD COLUMN `misc_value` int NOT NULL DEFAULT 0 COMMENT ''vanilla effectMiscValue, must equal the client one for aura effects'' AFTER `points_per_level`', 'SELECT 1');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;
SET @sql := IF((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'classic_spell_value' AND COLUMN_NAME = 'points_per_combo') = 0,
    'ALTER TABLE `classic_spell_value` ADD COLUMN `points_per_combo` float NOT NULL DEFAULT 0 COMMENT ''vanilla effectPointsPerComboPoint'' AFTER `misc_value`', 'SELECT 1');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @sql := IF((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'classic_spell_value' AND COLUMN_NAME = 'bonus_coeff') = 0,
    'ALTER TABLE `classic_spell_value` ADD COLUMN `bonus_coeff` float NOT NULL DEFAULT -1 COMMENT ''vanilla effectBonusCoefficient (spell / healing power factor), -1 = keep the client one'' AFTER `points_per_combo`', 'SELECT 1');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

CREATE TABLE IF NOT EXISTS `classic_spell_cost` (
  `spell_id` int unsigned NOT NULL,
  `mana_cost` int NOT NULL DEFAULT 0 COMMENT 'vanilla manaCost',
  `mana_cost_pct` int NOT NULL DEFAULT 0 COMMENT 'vanilla manaCostPercentage, of the caster base mana',
  `spell_name` varchar(100) NOT NULL DEFAULT '',
  PRIMARY KEY (`spell_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Classic 1.60: vanilla 1.12 mana cost of the legacy spells';

SET @sql := IF((SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'player_classlevelstats' AND COLUMN_NAME = 'basemana') = 0,
    'ALTER TABLE `player_classlevelstats` ADD COLUMN `basemana` int unsigned NOT NULL DEFAULT 0 COMMENT ''vanilla base mana (VMaNGOS), 0 = client game table''', 'SELECT 1');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;
