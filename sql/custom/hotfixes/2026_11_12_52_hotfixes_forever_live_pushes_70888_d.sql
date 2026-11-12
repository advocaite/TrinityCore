-- WoW Forever: Blizzard's live hotfix pushes 114002-114015 (client 1.60.1.70888), after 2026_11_12_50 (which stops at 113997).
-- Records from the public mirror of the live hotfix file
-- (https://storage.googleapis.com/raidbots-static/wow/forever/enUS/DBCache.bin, read 2026-11-12 05:20). Blizzard's values, unchanged.
-- What changed live: Divine Spirit (14752, 14818, 14819) and Prayer of Spirit (27681, 27841) class options, Frost Nova (865, 6131,
-- 10230) spell flags, Glimmering Mithril Insignia (10418), two spell effects, quest bit flags for 94670 and 95161, and a few rows
-- live hid. Needs 2026_11_12_50 first.
-- Made with tools/full_forever/live_hotfix_sql.py. hotfix_data ids 2085000-2085999. Safe to run again.

DELETE FROM `item` WHERE `ID` IN (10418);
INSERT INTO `item` (`ID`,`ClassID`,`SubclassID`,`Material`,`InventoryType`,`SheatheType`,`ItemPetFoodID`,`SoundOverrideSubclassID`,`IconFileDataID`,`ItemGroupSoundsID`,`ContentTuningID`,`ModifiedCraftingReagentItemID`,`Unknown1200`,`CraftingQualityID`,`ItemSquishEraID`,`RecraftReagentCountPercentage`,`OrderSource`,`VerifiedBuild`) VALUES
(10418,4,0,4,12,0,0,-1,133435,8,0,0,0,0,0,0,0,70888);
DELETE FROM `item_sparse` WHERE `ID` IN (10418);
INSERT INTO `item_sparse` (`ID`,`Description`,`Display3`,`Display2`,`Display1`,`Display`,`ExpansionID`,`DmgVariance`,`LimitCategory`,`DurationInInventory`,`QualityModifier`,`BagFamily`,`StartQuestID`,`LanguageID`,`ItemRange`,`StatPercentageOfSocket1`,`StatPercentageOfSocket2`,`StatPercentageOfSocket3`,`StatPercentageOfSocket4`,`StatPercentageOfSocket5`,`StatPercentageOfSocket6`,`StatPercentageOfSocket7`,`StatPercentageOfSocket8`,`StatPercentageOfSocket9`,`StatPercentageOfSocket10`,`StatPercentEditor1`,`StatPercentEditor2`,`StatPercentEditor3`,`StatPercentEditor4`,`StatPercentEditor5`,`StatPercentEditor6`,`StatPercentEditor7`,`StatPercentEditor8`,`StatPercentEditor9`,`StatPercentEditor10`,`StatModifierBonusStat1`,`StatModifierBonusStat2`,`StatModifierBonusStat3`,`StatModifierBonusStat4`,`StatModifierBonusStat5`,`StatModifierBonusStat6`,`StatModifierBonusStat7`,`StatModifierBonusStat8`,`StatModifierBonusStat9`,`StatModifierBonusStat10`,`Stackable`,`MaxCount`,`MinReputation`,`RequiredAbility`,`AllowableRace1`,`AllowableRace2`,`SellPrice`,`BuyPrice`,`VendorStackCount`,`PriceVariance`,`PriceRandomValue`,`Flags1`,`Flags2`,`Flags3`,`Flags4`,`Flags5`,`FactionRelated`,`ModifiedCraftingReagentItemID`,`ContentTuningID`,`PlayerLevelToItemLevelCurveID`,`ItemLevelOffsetCurveID`,`ItemLevelOffsetItemLevel`,`ItemSquishEraID`,`ItemNameDescriptionID`,`RequiredTransmogHoliday`,`RequiredHoliday`,`GemProperties`,`SocketMatchEnchantmentId`,`TotemCategoryID`,`InstanceBound`,`ZoneBound1`,`ZoneBound2`,`ItemSet`,`LockID`,`PageID`,`ItemDelay`,`MinFactionID`,`RequiredSkillRank`,`RequiredSkill`,`ItemLevel`,`AllowableClass`,`ArtifactID`,`SpellWeight`,`SpellWeightCategory`,`SocketType1`,`SocketType2`,`SocketType3`,`SheatheType`,`Material`,`PageMaterialID`,`Bonding`,`DamageDamageType`,`ContainerSlots`,`RequiredPVPMedal`,`RequiredPVPRank`,`RequiredLevel`,`InventoryType`,`OverallQualityID`,`AmmunitionType`,`VerifiedBuild`) VALUES
(10418,'Courage, Honor, and above all, Patience','','','','Glimmering Mithril Insignia',0,0,0,0,0,1048576,0,0,0,0,0,0,0,0,0,0,0,0,0,62500,0,0,0,0,0,0,0,0,0,50,-1,-1,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,16464,65859,1,1,1,524288,8196,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,225,164,50,-1,0,0,0,0,0,0,0,4,0,1,0,0,0,0,0,12,2,0,70888);
DELETE FROM `quest_v2` WHERE `ID` IN (94670,95161);
INSERT INTO `quest_v2` (`ID`,`UniqueBitFlag`,`UiQuestDetailsTheme`,`VerifiedBuild`) VALUES
(94670,68160,0,70888),
(95161,68549,0,70888);
DELETE FROM `spell_class_options` WHERE `ID` IN (45078,45079,45288,47007,47364);
INSERT INTO `spell_class_options` (`ID`,`SpellID`,`ModalNextSpell`,`SpellClassSet`,`SpellClassMask1`,`SpellClassMask2`,`SpellClassMask3`,`SpellClassMask4`,`VerifiedBuild`) VALUES
(45078,14818,0,6,-2147483648,4194304,0,0,70888),
(45079,14819,0,6,-2147483648,4194304,0,0,70888),
(45288,14752,0,6,-2147483648,4194304,0,0,70888),
(47007,27681,0,6,-2147483648,4194304,0,0,70888),
(47364,27841,0,6,-2147483648,4194304,0,0,70888);
DELETE FROM `spell_effect` WHERE `ID` IN (1272474,1273052);
INSERT INTO `spell_effect` (`ID`,`EffectAura`,`DifficultyID`,`EffectIndex`,`Effect`,`EffectAmplitude`,`EffectAttributes`,`EffectAuraPeriod`,`EffectBonusCoefficient`,`EffectChainAmplitude`,`EffectChainTargets`,`EffectItemType`,`EffectMechanic`,`EffectPointsPerResource`,`EffectPosFacing`,`EffectRealPointsPerLevel`,`EffectTriggerSpell`,`BonusCoefficientFromAP`,`PvpMultiplier`,`Coefficient`,`Variance`,`ResourceCoefficient`,`GroupSizeBasePointsCoefficient`,`EffectBasePoints`,`ScalingClass`,`TargetNodeGraph`,`EffectMiscValue1`,`EffectMiscValue2`,`EffectRadiusIndex1`,`EffectRadiusIndex2`,`EffectSpellClassMask1`,`EffectSpellClassMask2`,`EffectSpellClassMask3`,`EffectSpellClassMask4`,`ImplicitTarget1`,`ImplicitTarget2`,`SpellID`,`VerifiedBuild`) VALUES
(1272474,108,0,0,6,0,0,0,0,1,0,0,0,0,0,0,0,0,1,0,0,0,1,-100,0,0,14,0,0,0,424435563,1656815653,1,1048576,1,0,1262312,70888),
(1273052,256,0,0,6,0,0,0,1,1,0,0,0,0,0,0,0,0,1,0,0,0,1,-100,0,0,0,0,0,0,264,4194304,0,0,1,0,1262650,70888);
DELETE FROM `spell_misc` WHERE `ID` IN (314588,318832,322340);
INSERT INTO `spell_misc` (`ID`,`Attributes1`,`Attributes2`,`Attributes3`,`Attributes4`,`Attributes5`,`Attributes6`,`Attributes7`,`Attributes8`,`Attributes9`,`Attributes10`,`Attributes11`,`Attributes12`,`Attributes13`,`Attributes14`,`Attributes15`,`Attributes16`,`Attributes17`,`DifficultyID`,`CastingTimeIndex`,`DurationIndex`,`PvPDurationIndex`,`RangeIndex`,`SchoolMask`,`Speed`,`LaunchDelay`,`MinDuration`,`SpellIconFileDataID`,`ActiveIconFileDataID`,`ContentTuningID`,`ShowFutureSpellPlayerConditionID`,`SpellVisualScript`,`ActiveSpellVisualScript`,`SpellID`,`VerifiedBuild`) VALUES
(314588,1073807360,136,0,67108864,0,0,2097152,1073741824,0,0,0,0,2,128,64,8192,0,0,1,31,0,1,16,0,0,0,135848,0,0,0,0,0,865,70888),
(318832,1073807360,136,0,67108864,0,0,2097152,1073741824,0,0,0,0,2,128,64,8192,0,0,1,31,0,1,16,0,0,0,135848,0,0,0,0,0,6131,70888),
(322340,1073807360,136,0,67108864,0,0,2097152,1073741824,0,0,0,0,2,128,64,8192,0,0,1,31,0,1,16,0,0,0,135848,0,0,0,0,0,10230,70888);
DELETE FROM `hotfix_data` WHERE `Id` BETWEEN 2085000 AND 2085999;
DELETE FROM `hotfix_data` WHERE (`TableHash`,`RecordId`) IN ((1344507586,10418),(428746933,10418),(2442913102,10418),(3465080266,10418),(986198281,94670),(986198281,95161),(680438657,45078),(680438657,45079),(680438657,45288),(680438657,47007),(680438657,47364),(4030871717,1272474),(4030871717,1273052),(3322146344,314588),(3322146344,318832),(3322146344,322340),(1417785224,10418));
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(2085000,1397805000,1344507586,10418,1,70888),
(2085001,1397805001,428746933,10418,3,70888),
(2085002,1397805002,2442913102,10418,1,70888),
(2085003,1397805003,3465080266,10418,3,70888),
(2085004,1397805004,986198281,94670,1,70888),
(2085005,1397805005,986198281,95161,1,70888),
(2085006,1397805006,680438657,45078,1,70888),
(2085007,1397805007,680438657,45079,1,70888),
(2085008,1397805008,680438657,45288,1,70888),
(2085009,1397805009,680438657,47007,1,70888),
(2085010,1397805010,680438657,47364,1,70888),
(2085011,1397805011,4030871717,1272474,1,70888),
(2085012,1397805012,4030871717,1273052,1,70888),
(2085013,1397805013,3322146344,314588,1,70888),
(2085014,1397805014,3322146344,318832,1,70888),
(2085015,1397805015,3322146344,322340,1,70888),
(2085016,1397805016,1417785224,10418,3,70888);
