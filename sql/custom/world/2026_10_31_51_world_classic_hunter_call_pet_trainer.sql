-- Classic 1.60: Call Pet (883) has no trainer row, so no trainer offers it. Every hunter class trainer (every trainer that teaches
-- Hunter's Mark, 1130) now teaches it at level 10 for 10 copper; a hunter who already knows it sees it as known. Level and cost are the classic values, not
-- sniffed: the sniffed trainer lists do not carry the spell. Bots still learn it with the taming quest spells as before.
-- From Trixx34 (Trixx34/Trinity-Forever-Modules, commit e2e4dc69, GPL-2.0). Safe to run again.
DELETE FROM `trainer_spell` WHERE `SpellId` = 883;
INSERT INTO `trainer_spell` (`TrainerId`, `SpellId`, `MoneyCost`, `ReqSkillLine`, `ReqSkillRank`, `ReqAbility1`, `ReqAbility2`, `ReqAbility3`, `ReqLevel`, `VerifiedBuild`)
SELECT DISTINCT `TrainerId`, 883, 10, 0, 0, 0, 0, 0, 10, 0 FROM `trainer_spell` WHERE `SpellId` = 1130;
