-- WoW Forever launch (Blizzard development notes 2026-11-03): trainers stop teaching what Forever removes or turns into a talent.
-- Run by hand on launch day, after the launch client / live hotfixes that remove these spells are in (not before: until then the
-- spells still exist and nothing replaces them). Not applied by an install, an update or the DB updater. Safe to run again.
--   Mage: Frost Blast 1326053-1326055 (removed).
--   Shaman: Lava Burst 1238299, 1238300, 1326047 (removed, a talent takes its place).
--   Paladin: Blessing of Salvation 1038, Greater Blessing of Salvation 25895, Blessing of Light 19977-19979, Greater Blessing of
--     Light 25890 (removed); Righteous Fury 25780 trains at level 10 (was 16).
--   Druid: Lacerate stays: it is a Feral talent now, and its higher ranks still train after it (world 2026_11_03_42).
-- The rows are kept in `forever_launch_trainer_backup`; 01_undo.sql puts them back.
CREATE TABLE IF NOT EXISTS `forever_launch_trainer_backup` LIKE `trainer_spell`;
INSERT IGNORE INTO `forever_launch_trainer_backup` SELECT * FROM `trainer_spell`
 WHERE `SpellId` IN (1326053,1326054,1326055,1238299,1238300,1326047,1038,25895,19977,19978,19979,25890,25780);
DELETE FROM `trainer_spell`
 WHERE `SpellId` IN (1326053,1326054,1326055,1238299,1238300,1326047,1038,25895,19977,19978,19979,25890);
UPDATE `trainer_spell` SET `ReqLevel` = 10 WHERE `SpellId` = 25780;
