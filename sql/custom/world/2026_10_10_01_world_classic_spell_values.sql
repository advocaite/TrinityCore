DROP TABLE IF EXISTS `classic_creature_spell_value`;
-- Classic 1.60: table for the vanilla (1.12) spell values (classic_spell_value). No rows here: they are built from the server owner's own VMaNGOS dump by
-- tools/classic_spell_values/build_vanilla_data.py (see tools/classic_spell_values/README.md). An empty table changes nothing. Safe to run again.
CREATE TABLE IF NOT EXISTS `classic_spell_value` (
  `spell_id` int unsigned NOT NULL,
  `effect_index` tinyint unsigned NOT NULL,
  `effect` smallint unsigned NOT NULL COMMENT 'SpellEffects, must equal the client effect at that index',
  `apply_aura` smallint unsigned NOT NULL DEFAULT 0 COMMENT 'AuraType of an aura effect, must equal the client one',
  `base_points` int NOT NULL COMMENT 'vanilla base points; the die comes on top',
  `die_sides` int NOT NULL DEFAULT 0 COMMENT 'vanilla die sides: 0 none, n adds 1..n',
  `points_per_level` float NOT NULL DEFAULT 0 COMMENT 'vanilla real points per level',
  `spell_name` varchar(100) NOT NULL DEFAULT '',
  PRIMARY KEY (`spell_id`,`effect_index`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Classic 1.60: vanilla 1.12 spell values for every caster';
