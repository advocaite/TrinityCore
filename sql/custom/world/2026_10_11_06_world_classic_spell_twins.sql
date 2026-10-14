-- Classic 1.60: table for the Forever twins of vanilla spells (classic_spell_twin). No rows here: they are built from the server owner's own VMaNGOS dump by
-- tools/classic_spell_values/build_vanilla_data.py (see tools/classic_spell_values/README.md). An empty table changes nothing. Safe to run again.
CREATE TABLE IF NOT EXISTS `classic_spell_twin` (`spell_id` int unsigned NOT NULL, `vanilla_spell_id` int unsigned NOT NULL, `spell_name` varchar(100) NOT NULL DEFAULT '', PRIMARY KEY (`spell_id`)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
