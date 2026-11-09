-- Undo of 01_world_trainers_launch.sql: the trainer rows as they were. Safe to run again.
DELETE FROM `trainer_spell`
 WHERE `SpellId` IN (1326053,1326054,1326055,1238299,1238300,1326047,1038,25895,19977,19978,19979,25890,25780);
INSERT INTO `trainer_spell` SELECT * FROM `forever_launch_trainer_backup`;
