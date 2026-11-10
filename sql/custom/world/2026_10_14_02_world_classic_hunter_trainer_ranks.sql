-- Classic 1.60: hunter trainers sell Aimed Shot (Rank 1) and Counterattack (Rank 2).
-- The official hunter trainer (trainer 20, "Hello, hunter! Ready for some training?", sniff build 70124) teaches Aimed Shot
-- Rank 1 (19434, level 20, 2100 copper) and the Classic-only Counterattack Rank 2 (1242634, level 30, 7600 copper, needs
-- Counterattack Rank 1 19306). The class trainers imported from the vanilla data (trainer 1000000 + creature entry) only have
-- Aimed Shot Ranks 2-6 and Counterattack Ranks 3-4, so the first ranks could not be learned and the later ranks were stuck behind them.
-- Every trainer that teaches Aimed Shot Rank 2 gets the two missing rows.
DELETE FROM `trainer_spell` WHERE `SpellId` IN (19434, 1242634) AND `TrainerId` IN (SELECT `TrainerId` FROM (SELECT DISTINCT `TrainerId` FROM `trainer_spell` WHERE `SpellId` = 20900) AS `t`);
INSERT INTO `trainer_spell` (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`)
SELECT DISTINCT `TrainerId`, 19434, 2100, 0, 0, 0, 0, 0, 20, 70124 FROM `trainer_spell` WHERE `SpellId` = 20900;
INSERT INTO `trainer_spell` (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`)
SELECT DISTINCT `TrainerId`, 1242634, 7600, 0, 0, 19306, 0, 0, 30, 70124 FROM `trainer_spell` WHERE `SpellId` = 20900;
