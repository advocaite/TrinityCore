-- Classic 1.60: table for the vanilla creature melee numbers (creature_classic_melee). No rows here: they are built from the server owner's own VMaNGOS dump by
-- tools/classic_spell_values/build_vanilla_data.py (see tools/classic_spell_values/README.md). An empty table changes nothing. Safe to run again.
CREATE TABLE IF NOT EXISTS `creature_classic_melee` (
  `entry` int unsigned NOT NULL, `dmg_min` float NOT NULL, `dmg_max` float NOT NULL, `attack_ms` int unsigned NOT NULL, `ranged_attack_ms` int unsigned NOT NULL,
  PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
