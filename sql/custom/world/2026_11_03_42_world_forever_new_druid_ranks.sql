-- WoW Forever (live hotfixes, push 113283/113448-113449): new druid ranks on the trainers.
--   Ferocious Bite gets a new rank 1 at level 24 (1328618); the old ranks move up one (22568 is rank 2 at 32).
--   Lacerate is now a Feral talent (rank 1 = 1328617, from the talent). Rank 2 (1328616) trains at 34 and needs the talent;
--   the old trainer ranks 414644, 1235826, 1235827 follow it (414644 needed Dire Bear Form 9634, now rank 2).
-- Levels from live SpellLevels; prices between the neighbouring ranks (Forever's are not known). Safe to run again.
DELETE FROM `trainer_spell` WHERE `SpellId` IN (1328618,1328616);
INSERT INTO `trainer_spell` (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`)
SELECT DISTINCT `TrainerId`, 1328618, 4000, 0, 0, 0, 0, 0, 24, 0 FROM `trainer_spell` WHERE `SpellId` = 22568;
INSERT INTO `trainer_spell` (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`)
SELECT DISTINCT `TrainerId`, 1328616, 9000, 0, 0, 1328617, 0, 0, 34, 0 FROM `trainer_spell` WHERE `SpellId` = 414644;
UPDATE `trainer_spell` SET `ReqAbility1` = 1328616 WHERE `SpellId` = 414644;
