-- Ghost Wolf: since build 70888 live shaman trainers (Siln Skychaser, Sian'tsu) teach 1238640 instead of 2645, same price and
-- level. 1238640 has the same form and +40% speed but no "Only useable outdoors". Every trainer row for 2645 moves to 1238640;
-- known spells and action bars move in characters 2026_11_13_80. Data from Efficient Games (https://efficient.games), licensed
-- under CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/); source github.com/JIVESCORP/eg-link-output-wowf, build
-- 1.60.1.70888. Changed: applied to our trainer rows. Safe to run again.
INSERT IGNORE INTO `trainer_spell` (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`)
SELECT `TrainerId`,1238640,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,70888 FROM `trainer_spell` WHERE `SpellId`=2645;
DELETE FROM `trainer_spell` WHERE `SpellId`=2645;
UPDATE `trainer_spell` SET `ReqAbility1`=1238640 WHERE `ReqAbility1`=2645;
UPDATE `trainer_spell` SET `ReqAbility2`=1238640 WHERE `ReqAbility2`=2645;
UPDATE `trainer_spell` SET `ReqAbility3`=1238640 WHERE `ReqAbility3`=2645;
