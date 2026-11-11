-- WoW Forever: 9 items that still had no name or stats in our data, from an English player cache sent on 2026-11-10 evening
-- (Cache\ADB\enUS\DBCache.bin, build 70862, Blizzard's live realm): Item + ItemSparse records exactly as Blizzard sends them.
-- 8 drop from our loot: Draconian Deflector, Starfire Tiara, Flaming Band, Warmaster Legguards, Dragoneye Coif, Seeping Willow,
-- Cho'Rush's Blade, Stratholme Militia Shoulderguard. 280081 Sad-Sack Sandals is a Forever item nothing of ours drops yet.
-- Made with tools/full_forever/live_hotfix_sql.py. hotfix_data ids 2061000-2061999. Safe to run again.

DELETE FROM `item` WHERE `ID` IN (12602,12604,12926,12935,12953,12969,18484,18742,280081);
INSERT INTO `item` (`ID`,`ClassID`,`SubclassID`,`Material`,`InventoryType`,`SheatheType`,`ItemPetFoodID`,`SoundOverrideSubclassID`,`IconFileDataID`,`ItemGroupSoundsID`,`ContentTuningID`,`ModifiedCraftingReagentItemID`,`Unknown1200`,`CraftingQualityID`,`ItemSquishEraID`,`RecraftReagentCountPercentage`,`OrderSource`,`VerifiedBuild`) VALUES
(12602,4,6,1,14,4,0,-1,134966,9,0,0,0,0,0,0,0,70862),
(12604,4,1,7,1,0,0,-1,132768,7,0,0,0,0,0,0,0,70862),
(12926,4,0,5,11,0,0,-1,135926,14,0,0,0,0,0,0,0,70862),
(12935,4,4,6,7,0,0,-1,134584,11,0,0,0,0,0,0,0,70862),
(12953,4,3,5,1,0,0,-1,133141,10,0,0,0,0,0,0,0,70862),
(12969,2,5,2,17,1,0,-1,133054,9,0,0,0,0,0,0,0,70862),
(18484,2,7,1,13,3,0,-1,135640,9,0,0,0,0,0,0,0,70862),
(18742,4,3,5,3,0,0,-1,135045,10,0,0,0,0,0,0,0,70862),
(280081,4,1,7,8,0,0,-1,0,7,0,0,0,0,0,0,0,70862);
DELETE FROM `item_sparse` WHERE `ID` IN (12602,12604,12926,12935,12953,12969,18484,18742,280081);
INSERT INTO `item_sparse` (`ID`,`Description`,`Display3`,`Display2`,`Display1`,`Display`,`ExpansionID`,`DmgVariance`,`LimitCategory`,`DurationInInventory`,`QualityModifier`,`BagFamily`,`StartQuestID`,`LanguageID`,`ItemRange`,`StatPercentageOfSocket1`,`StatPercentageOfSocket2`,`StatPercentageOfSocket3`,`StatPercentageOfSocket4`,`StatPercentageOfSocket5`,`StatPercentageOfSocket6`,`StatPercentageOfSocket7`,`StatPercentageOfSocket8`,`StatPercentageOfSocket9`,`StatPercentageOfSocket10`,`StatPercentEditor1`,`StatPercentEditor2`,`StatPercentEditor3`,`StatPercentEditor4`,`StatPercentEditor5`,`StatPercentEditor6`,`StatPercentEditor7`,`StatPercentEditor8`,`StatPercentEditor9`,`StatPercentEditor10`,`StatModifierBonusStat1`,`StatModifierBonusStat2`,`StatModifierBonusStat3`,`StatModifierBonusStat4`,`StatModifierBonusStat5`,`StatModifierBonusStat6`,`StatModifierBonusStat7`,`StatModifierBonusStat8`,`StatModifierBonusStat9`,`StatModifierBonusStat10`,`Stackable`,`MaxCount`,`MinReputation`,`RequiredAbility`,`AllowableRace1`,`AllowableRace2`,`SellPrice`,`BuyPrice`,`VendorStackCount`,`PriceVariance`,`PriceRandomValue`,`Flags1`,`Flags2`,`Flags3`,`Flags4`,`Flags5`,`FactionRelated`,`ModifiedCraftingReagentItemID`,`ContentTuningID`,`PlayerLevelToItemLevelCurveID`,`ItemLevelOffsetCurveID`,`ItemLevelOffsetItemLevel`,`ItemSquishEraID`,`ItemNameDescriptionID`,`RequiredTransmogHoliday`,`RequiredHoliday`,`GemProperties`,`SocketMatchEnchantmentId`,`TotemCategoryID`,`InstanceBound`,`ZoneBound1`,`ZoneBound2`,`ItemSet`,`LockID`,`PageID`,`ItemDelay`,`MinFactionID`,`RequiredSkillRank`,`RequiredSkill`,`ItemLevel`,`AllowableClass`,`ArtifactID`,`SpellWeight`,`SpellWeightCategory`,`SocketType1`,`SocketType2`,`SocketType3`,`SheatheType`,`Material`,`PageMaterialID`,`Bonding`,`DamageDamageType`,`ContainerSlots`,`RequiredPVPMedal`,`RequiredPVPRank`,`RequiredLevel`,`InventoryType`,`OverallQualityID`,`AmmunitionType`,`VerifiedBuild`) VALUES
(12602,'','','','','Draconian Deflector',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3500,4000,5000,0,0,0,0,0,0,0,7,51,12,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,35212,176064,1,1,1,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,63,-1,0,0,0,0,0,0,4,1,0,1,0,0,0,0,58,14,3,0,70862),
(12604,'','','','','Starfire Tiara',0,0.2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2941,10882,2941,2941,0,0,0,0,0,0,6,89,51,5,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,14368,71844,1,1,1,0,4,0,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,60,-1,0,0,0,0,0,0,0,7,0,1,0,0,0,0,55,1,3,0,70862),
(12926,'','','','','Flaming Band',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,12000,0,0,0,0,0,0,0,0,0,85,-1,-1,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,14907,59630,1,1,1,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,61,-1,0,0,0,0,0,0,0,5,0,1,0,0,0,0,56,11,3,0,70862),
(12935,'','','','','Warmaster Legguards',0,0.2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4444,3611,6667,1666,0,0,0,0,0,0,7,4,13,14,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,23196,115982,1,1,1,0,4,0,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,63,-1,0,0,0,0,0,0,0,6,0,1,0,0,0,0,58,7,3,0,70862),
(12953,'','','','','Dragoneye Coif',0,0.2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5000,11176,0,0,0,0,0,0,0,0,7,38,-1,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,22374,111874,1,1,1,0,4,0,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,60,-1,0,0,0,0,0,0,0,5,0,1,0,0,0,0,55,1,3,0,70862),
(12969,'','','','','Seeping Willow',0,0.4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,70884,354420,1,1,1,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3600,0,0,0,63,-1,0,0,0,0,0,0,1,2,0,1,0,0,0,0,58,17,3,0,70862),
(18484,'','','','','Cho''Rush''s Blade',0,0.6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,13333,20000,0,0,0,0,0,0,0,0,38,125,-1,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,53822,269110,1,1,1,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2400,0,0,0,61,-1,0,0,0,0,0,0,3,1,0,1,0,0,0,0,56,13,3,0,70862),
(18742,'','','','','Stratholme Militia Shoulderguard',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8462,3462,2307,0,0,0,0,0,0,0,3,7,5,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,22503,112518,1,1,1,0,4,0,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,60,-1,0,0,0,0,0,0,0,5,0,2,0,0,0,0,55,3,3,0,70862),
(280081,'','','','','Sad-Sack Sandals',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2916,5259,0,0,0,0,0,0,0,-1,32,7,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,14751,73755,1,1,0.9885,0,0,0,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,57,-1,0,0,0,0,0,0,0,7,0,1,0,0,0,0,52,8,3,0,70862);
DELETE FROM `hotfix_data` WHERE `Id` BETWEEN 2061000 AND 2061999;
DELETE FROM `hotfix_data` WHERE (`TableHash`,`RecordId`) IN ((1344507586,12602),(1344507586,12604),(1344507586,12926),(1344507586,12935),(1344507586,12953),(1344507586,12969),(1344507586,18484),(1344507586,18742),(1344507586,280081),(2442913102,12602),(2442913102,12604),(2442913102,12926),(2442913102,12935),(2442913102,12953),(2442913102,12969),(2442913102,18484),(2442913102,18742),(2442913102,280081));
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(2061000,1397781000,1344507586,12602,1,70862),
(2061001,1397781001,1344507586,12604,1,70862),
(2061002,1397781002,1344507586,12926,1,70862),
(2061003,1397781003,1344507586,12935,1,70862),
(2061004,1397781004,1344507586,12953,1,70862),
(2061005,1397781005,1344507586,12969,1,70862),
(2061006,1397781006,1344507586,18484,1,70862),
(2061007,1397781007,1344507586,18742,1,70862),
(2061008,1397781008,1344507586,280081,1,70862),
(2061009,1397781009,2442913102,12602,1,70862),
(2061010,1397781010,2442913102,12604,1,70862),
(2061011,1397781011,2442913102,12926,1,70862),
(2061012,1397781012,2442913102,12935,1,70862),
(2061013,1397781013,2442913102,12953,1,70862),
(2061014,1397781014,2442913102,12969,1,70862),
(2061015,1397781015,2442913102,18484,1,70862),
(2061016,1397781016,2442913102,18742,1,70862),
(2061017,1397781017,2442913102,280081,1,70862);
