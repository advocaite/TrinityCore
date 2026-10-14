-- Classic 1.60: table for the vanilla spell timing (classic_spell_info). No rows here: they are built from the server owner's own VMaNGOS dump by
-- tools/classic_spell_values/build_vanilla_data.py (see tools/classic_spell_values/README.md). An empty table changes nothing. Safe to run again.
CREATE TABLE IF NOT EXISTS `classic_spell_info` (
  `spell_id` int unsigned NOT NULL, `cast_ms` int DEFAULT NULL, `recovery_ms` int DEFAULT NULL, `category_recovery_ms` int DEFAULT NULL,
  `duration_ms` int DEFAULT NULL, `range_max` float DEFAULT NULL, `radius1` float DEFAULT NULL, `radius2` float DEFAULT NULL, `radius3` float DEFAULT NULL,
  `spell_name` varchar(100) NOT NULL DEFAULT '', PRIMARY KEY (`spell_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
