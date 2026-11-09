-- WoW Forever launch: characters forget the spells Forever removed (characters DB). Run by hand on launch day, together with
-- world 01, and only after the launch client is in. Spells that became talents (Lacerate) are left alone: the talent respec
-- handles those. Rows are kept in `forever_launch_spell_backup`; 02_undo.sql puts them back. Safe to run again.
CREATE TABLE IF NOT EXISTS `forever_launch_spell_backup` LIKE `character_spell`;
INSERT IGNORE INTO `forever_launch_spell_backup` SELECT * FROM `character_spell`
 WHERE `spell` IN (1326053,1326054,1326055,1238299,1238300,1326047,1038,25895,19977,19978,19979,25890);
DELETE FROM `character_spell`
 WHERE `spell` IN (1326053,1326054,1326055,1238299,1238300,1326047,1038,25895,19977,19978,19979,25890);
