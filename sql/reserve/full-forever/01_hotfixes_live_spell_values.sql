-- FULL FOREVER RESERVE (not applied by any install or update; see README.md here).
-- Live WoW Forever values for spells where we keep the vanilla value for now (Karrz 10-30: hold vanilla until Forever's gear and
-- balance are there, then switch). Blizzard's live rows after 70685, from the live hotfix cache wowsims/forever (MIT,
-- https://github.com/wowsims/forever) uses. Pair with 02_world_drop_vanilla_holds.sql, or the vanilla value rows still win.
--   Aspect of the Hawk 13165/14318/14319/14320/14321: 20/35/50/70/90 -> 15/22/30/38/47 ranged attack power.
--   Trueshot Aura 19506/20905: 50/75 -> 30/40; Forever ranks 1299346/1299348: 30/40 -> 15/22.
--   Lesser Intellect 3166: 6 -> 3 Intellect.
-- Own hotfix_data ids 2001500-2001559. Undo: 01_undo.sql. Safe to run again.
DELETE FROM `spell_effect` WHERE `ID` IN (690400,691884,691886,692212,692607,697417,698474,1323534,1323538);
INSERT INTO `spell_effect` (`ID`,`EffectAura`,`DifficultyID`,`EffectIndex`,`Effect`,`EffectAmplitude`,`EffectAttributes`,`EffectAuraPeriod`,`EffectBonusCoefficient`,`EffectChainAmplitude`,`EffectChainTargets`,`EffectItemType`,`EffectMechanic`,`EffectPointsPerResource`,`EffectPosFacing`,`EffectRealPointsPerLevel`,`EffectTriggerSpell`,`BonusCoefficientFromAP`,`PvpMultiplier`,`Coefficient`,`Variance`,`ResourceCoefficient`,`GroupSizeBasePointsCoefficient`,`EffectBasePoints`,`ScalingClass`,`TargetNodeGraph`,`EffectMiscValue1`,`EffectMiscValue2`,`EffectRadiusIndex1`,`EffectRadiusIndex2`,`EffectSpellClassMask1`,`EffectSpellClassMask2`,`EffectSpellClassMask3`,`EffectSpellClassMask4`,`ImplicitTarget1`,`ImplicitTarget2`,`SpellID`,`VerifiedBuild`) VALUES
(690400,124,0,0,6,0,0,0,0,1,0,0,0,0,0,0,0,0,1,0,0,0,1,15,0,0,0,0,0,0,0,0,0,0,1,0,13165,70685),
(691884,124,0,0,6,0,0,0,0,1,0,0,0,0,0,0,0,0,1,0,0,0,1,22,0,0,0,0,0,0,0,0,0,0,1,0,14318,70685),
(691886,124,0,0,6,0,0,0,0,1,0,0,0,0,0,0,0,0,1,0,0,0,1,30,0,0,0,0,0,0,0,0,0,0,1,0,14319,70685),
(692212,124,0,0,6,0,0,0,0,1,0,0,0,0,0,0,0,0,1,0,0,0,1,38,0,0,0,0,0,0,0,0,0,0,1,0,14320,70685),
(692607,124,0,0,6,0,0,0,0,1,0,0,0,0,0,0,0,0,1,0,0,0,1,47,0,0,0,0,0,0,0,0,0,0,1,0,14321,70685),
(697417,124,0,0,65,0,0,0,0,1,0,0,0,0,0,0,0,0,1,0,0,0,1,30,0,0,0,0,11,11,0,0,0,0,1,0,19506,70685),
(698474,124,0,0,65,0,0,0,0,1,0,0,0,0,0,0,0,0,1,0,0,0,1,40,0,0,0,0,11,11,0,0,0,0,1,0,20905,70685),
(1323534,124,0,0,65,0,0,0,0,1,0,0,0,0,0,0,0,0,1,0,0,0,1,15,0,0,0,0,11,11,0,0,0,0,1,0,1299346,70685),
(1323538,124,0,0,65,0,0,0,0,1,0,0,0,0,0,0,0,0,1,0,0,0,1,22,0,0,0,0,11,11,0,0,0,0,1,0,1299348,70685);
DELETE FROM `hotfix_data` WHERE `Id` BETWEEN 2001500 AND 2001549;
DELETE FROM `hotfix_data` WHERE (`TableHash`,`RecordId`) IN ((4030871717,690400),(4030871717,691884),(4030871717,691886),(4030871717,692212),(4030871717,692607),(4030871717,697417),(4030871717,698474),(4030871717,1323534),(4030871717,1323538));
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(2001500,1397721500,4030871717,690400,1,70685),
(2001501,1397721501,4030871717,691884,1,70685),
(2001502,1397721502,4030871717,691886,1,70685),
(2001503,1397721503,4030871717,692212,1,70685),
(2001504,1397721504,4030871717,692607,1,70685),
(2001505,1397721505,4030871717,697417,1,70685),
(2001506,1397721506,4030871717,698474,1,70685),
(2001507,1397721507,4030871717,1323534,1,70685),
(2001508,1397721508,4030871717,1323538,1,70685);
DELETE FROM `spell_effect` WHERE `ID` IN (681806);
INSERT INTO `spell_effect` (`ID`,`EffectAura`,`DifficultyID`,`EffectIndex`,`Effect`,`EffectAmplitude`,`EffectAttributes`,`EffectAuraPeriod`,`EffectBonusCoefficient`,`EffectChainAmplitude`,`EffectChainTargets`,`EffectItemType`,`EffectMechanic`,`EffectPointsPerResource`,`EffectPosFacing`,`EffectRealPointsPerLevel`,`EffectTriggerSpell`,`BonusCoefficientFromAP`,`PvpMultiplier`,`Coefficient`,`Variance`,`ResourceCoefficient`,`GroupSizeBasePointsCoefficient`,`EffectBasePoints`,`ScalingClass`,`TargetNodeGraph`,`EffectMiscValue1`,`EffectMiscValue2`,`EffectRadiusIndex1`,`EffectRadiusIndex2`,`EffectSpellClassMask1`,`EffectSpellClassMask2`,`EffectSpellClassMask3`,`EffectSpellClassMask4`,`ImplicitTarget1`,`ImplicitTarget2`,`SpellID`,`VerifiedBuild`) VALUES
(681806,29,0,0,6,0,0,0,0,1,0,0,0,0,0,0,0,0,1,0,0,0,1,3,0,0,3,0,0,0,0,0,0,0,1,0,3166,70685);
DELETE FROM `hotfix_data` WHERE `Id` BETWEEN 2001550 AND 2001559;
DELETE FROM `hotfix_data` WHERE (`TableHash`,`RecordId`) IN ((4030871717,681806));
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(2001550,1397721550,4030871717,681806,1,70685);
