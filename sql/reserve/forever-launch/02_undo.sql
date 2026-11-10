-- Undo of 02_characters_unlearn_removed.sql (characters DB). Safe to run again.
INSERT IGNORE INTO `character_spell` SELECT * FROM `forever_launch_spell_backup`;
