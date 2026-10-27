-- Every warrior got the hidden passives Victorious State (DND) 402974 and Offensive State (DND) 1282735 at level 1:
-- SkillLineAbility 48279 / 57190 (Defense 95, ClassMask warrior, all races) have AcquireMethod 2 (learned automatically by
-- character level). So the Victorious proc (Victory Rush) and the Overpower proc showed from level 1, long before Victory Rush
-- (level 20) and Overpower (level 12) are trained. Same records with AcquireMethod 0 (every other field as in the 70532 DB2);
-- the world part teaches each passive with its spell (spell_learn_spell).
DELETE FROM `skill_line_ability` WHERE `ID` IN (48279, 57190);
INSERT INTO `skill_line_ability` (`AbilityVerb`, `AbilityAllVerb`, `ID`, `SkillLine`, `Spell`, `MinSkillLineRank`, `ClassMask`, `SupercedesSpell`, `AcquireMethod`, `TrivialSkillLineRankHigh`, `TrivialSkillLineRankLow`, `Flags`, `NumSkillUps`, `UniqueBit`, `TradeSkillCategoryID`, `SkillupSkillLineID`, `Field_5_5_4_67090_0141`, `Field_5_5_4_67090_0142`, `RaceMask1`, `RaceMask2`, `VerifiedBuild`) VALUES
('', '', 48279, 95, 402974, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, 0),
('', '', 57190, 95, 1282735, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, 0);
DELETE FROM `hotfix_data` WHERE `Id` = 2000110;
INSERT INTO `hotfix_data` (`Id`, `UniqueId`, `TableHash`, `RecordId`, `Status`, `VerifiedBuild`) VALUES
(2000110, 2000110, 4282664694, 48279, 1, 0),
(2000110, 2000111, 4282664694, 57190, 1, 0);
