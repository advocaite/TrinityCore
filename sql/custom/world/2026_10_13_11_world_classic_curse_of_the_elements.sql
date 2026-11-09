-- Classic 1.60: Curse of the Elements. The client has one rank (440892): -30 resistance and +4% damage taken for every magic school, 5 min. The
-- vanilla trainer spells (1490, 11721, 11722) are not in this client's data, so rank 3 is the vanilla curse: -75 fire and frost resistance and +10% fire
-- and frost damage taken (VMaNGOS 1.12), 5 min. The school mask of both effects goes from 126 to 20 (fire + frost) through classic_spell_effect, the
-- values are the vanilla ones. The table has the layout of tools/classic_spell_values/gen_spell_effects.py; its rows are added, never deleted, so a
-- generated file does not lose them. Safe to run again.
CREATE TABLE IF NOT EXISTS `classic_spell_effect` (
  `spell_id` int unsigned NOT NULL, `effect_index` tinyint unsigned NOT NULL, `apply_aura` smallint unsigned NOT NULL, `misc_value` int NOT NULL,
  `class_mask0` int unsigned NOT NULL, `class_mask1` int unsigned NOT NULL, `class_mask2` int unsigned NOT NULL, `class_mask3` int unsigned NOT NULL,
  `spell_name` varchar(100) NOT NULL DEFAULT '', PRIMARY KEY (`spell_id`,`effect_index`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

REPLACE INTO `classic_spell_effect` (`spell_id`,`effect_index`,`apply_aura`,`misc_value`,`class_mask0`,`class_mask1`,`class_mask2`,`class_mask3`,`spell_name`) VALUES
(440892,0,22,20,0,0,0,0,'Curse of the Elements'),
(440892,1,87,20,0,0,0,0,'Curse of the Elements');

REPLACE INTO `classic_spell_value` (`spell_id`,`effect_index`,`effect`,`apply_aura`,`base_points`,`die_sides`,`points_per_level`,`misc_value`,`points_per_combo`,`bonus_coeff`,`spell_name`) VALUES
(440892,0,6,22,-76,1,0,20,0,0,'Curse of the Elements'),
(440892,1,6,87,9,1,0,20,0,0,'Curse of the Elements');
