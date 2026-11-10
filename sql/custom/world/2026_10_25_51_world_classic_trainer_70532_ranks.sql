-- Classic 1.60 client 70532: the class trainers teach the new top ranks 70532 added, at the trainer price and level of the 70450 trainer
-- lists (SMSG_TRAINER_LIST, VerifiedBuild 70450): Arcane Wrath ranks 2-4 (1326080-1326082, after rank 1 1326079), Mana Tide Totem rank 4
-- (1325256, level 58) and Lava Burst rank 4 (1326047, level 60). Mana Tide Totem ranks 2-3 and Lava Burst ranks 2-3 move ten levels down to their
-- 70532 spell level (38/48 and 40/50) to make room for the new ranks. Every trainer that teaches the rank before gets the row. Safe to run again.
DELETE FROM `trainer_spell` WHERE `SpellId` IN (1326080, 1326081, 1326082, 1325256, 1326047);
INSERT INTO `trainer_spell` (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`)
SELECT DISTINCT `TrainerId`, 1326080, 11578, 0, 0, 1326079, 0, 0, 36, 70450 FROM `trainer_spell` WHERE `SpellId` = 10161;
INSERT INTO `trainer_spell` (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`)
SELECT DISTINCT `TrainerId`, 1326081, 23157, 0, 0, 1326080, 0, 0, 48, 70450 FROM `trainer_spell` WHERE `SpellId` = 10161;
INSERT INTO `trainer_spell` (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`)
SELECT DISTINCT `TrainerId`, 1326082, 32631, 0, 0, 1326081, 0, 0, 60, 70450 FROM `trainer_spell` WHERE `SpellId` = 10161;
INSERT INTO `trainer_spell` (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`)
SELECT DISTINCT `TrainerId`, 1325256, 1700, 0, 0, 17359, 0, 0, 58, 70450 FROM `trainer_spell` WHERE `SpellId` = 17359;
INSERT INTO `trainer_spell` (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`)
SELECT DISTINCT `TrainerId`, 1326047, 34000, 0, 0, 1238300, 0, 0, 60, 70450 FROM `trainer_spell` WHERE `SpellId` = 1238300;
UPDATE `trainer_spell` SET `ReqLevel` = 38 WHERE `SpellId` = 17354;
UPDATE `trainer_spell` SET `ReqLevel` = 48 WHERE `SpellId` = 17359;
UPDATE `trainer_spell` SET `ReqLevel` = 40 WHERE `SpellId` = 1238299;
UPDATE `trainer_spell` SET `ReqLevel` = 50 WHERE `SpellId` = 1238300;
