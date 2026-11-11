-- WoW Forever: 10 items that still had no name or stats in our data, from a Russian player's cache folder sent on 2026-11-10
-- evening (Cache\ADB\ruRU\DBCache.bin, build 70862, Blizzard's live realm): Item + ItemSparse records exactly as Blizzard sends
-- them, with the English name put in. 9 drop from our loot: Brain Hacker, Dal'Rend's Tribal Guardian, Dal'Rend's Sacred Charge,
-- Spiritshroud Leggings, Brigam Girdle, Unbridled Leggings, Ancient Bone Bow, Icy Tomb Spaulders, Faith Healer's Boots (names from
-- Questie's classic item data, github.com/Questie/QuestieDB, GPLv3). 280083 is a Forever item no open source names yet:
-- "Rifle of Insight" is our translation (tools/forever_cache/item_translations.tsv). Russian text in item_sparse_locale.
-- Made with tools/forever_cache/english_names.py. hotfix_data ids 2060000-2060999. Safe to run again.

DELETE FROM `item` WHERE `ID` IN (1263,12939,12940,12965,13142,18298,18680,18699,22247,280083);
INSERT INTO `item` (`ID`,`ClassID`,`SubclassID`,`Material`,`InventoryType`,`SheatheType`,`ItemPetFoodID`,`SoundOverrideSubclassID`,`IconFileDataID`,`ItemGroupSoundsID`,`ContentTuningID`,`ModifiedCraftingReagentItemID`,`Unknown1200`,`CraftingQualityID`,`ItemSquishEraID`,`RecraftReagentCountPercentage`,`OrderSource`,`VerifiedBuild`) VALUES
(1263,2,1,1,17,1,0,-1,135580,9,0,0,0,0,0,0,0,70862),
(12939,2,7,1,22,3,0,-1,135350,8,0,0,0,0,0,0,0,70862),
(12940,2,7,1,21,3,0,-1,135353,8,0,0,0,0,0,0,0,70862),
(12965,4,1,7,7,0,0,-1,134586,7,0,0,0,0,0,0,0,70862),
(13142,4,4,6,6,0,0,-1,132522,11,0,0,0,0,0,0,0,70862),
(18298,4,2,8,7,0,0,-1,134591,7,0,0,0,0,0,0,0,70862),
(18680,2,2,2,15,0,0,-1,135496,12,0,0,2,0,0,0,0,70862),
(18699,4,2,8,3,0,0,-1,135058,7,0,0,0,0,0,0,0,70862),
(22247,4,1,7,8,0,0,-1,132560,7,0,0,0,0,0,0,0,70862),
(280083,2,3,1,26,0,0,-1,0,8,0,0,3,0,0,0,0,70862);
DELETE FROM `item_sparse` WHERE `ID` IN (1263,12939,12940,12965,13142,18298,18680,18699,22247,280083);
INSERT INTO `item_sparse` (`ID`,`Description`,`Display3`,`Display2`,`Display1`,`Display`,`ExpansionID`,`DmgVariance`,`LimitCategory`,`DurationInInventory`,`QualityModifier`,`BagFamily`,`StartQuestID`,`LanguageID`,`ItemRange`,`StatPercentageOfSocket1`,`StatPercentageOfSocket2`,`StatPercentageOfSocket3`,`StatPercentageOfSocket4`,`StatPercentageOfSocket5`,`StatPercentageOfSocket6`,`StatPercentageOfSocket7`,`StatPercentageOfSocket8`,`StatPercentageOfSocket9`,`StatPercentageOfSocket10`,`StatPercentEditor1`,`StatPercentEditor2`,`StatPercentEditor3`,`StatPercentEditor4`,`StatPercentEditor5`,`StatPercentEditor6`,`StatPercentEditor7`,`StatPercentEditor8`,`StatPercentEditor9`,`StatPercentEditor10`,`StatModifierBonusStat1`,`StatModifierBonusStat2`,`StatModifierBonusStat3`,`StatModifierBonusStat4`,`StatModifierBonusStat5`,`StatModifierBonusStat6`,`StatModifierBonusStat7`,`StatModifierBonusStat8`,`StatModifierBonusStat9`,`StatModifierBonusStat10`,`Stackable`,`MaxCount`,`MinReputation`,`RequiredAbility`,`AllowableRace1`,`AllowableRace2`,`SellPrice`,`BuyPrice`,`VendorStackCount`,`PriceVariance`,`PriceRandomValue`,`Flags1`,`Flags2`,`Flags3`,`Flags4`,`Flags5`,`FactionRelated`,`ModifiedCraftingReagentItemID`,`ContentTuningID`,`PlayerLevelToItemLevelCurveID`,`ItemLevelOffsetCurveID`,`ItemLevelOffsetItemLevel`,`ItemSquishEraID`,`ItemNameDescriptionID`,`RequiredTransmogHoliday`,`RequiredHoliday`,`GemProperties`,`SocketMatchEnchantmentId`,`TotemCategoryID`,`InstanceBound`,`ZoneBound1`,`ZoneBound2`,`ItemSet`,`LockID`,`PageID`,`ItemDelay`,`MinFactionID`,`RequiredSkillRank`,`RequiredSkill`,`ItemLevel`,`AllowableClass`,`ArtifactID`,`SpellWeight`,`SpellWeightCategory`,`SocketType1`,`SocketType2`,`SocketType3`,`SheatheType`,`Material`,`PageMaterialID`,`Bonding`,`DamageDamageType`,`ContainerSlots`,`RequiredPVPMedal`,`RequiredPVPRank`,`RequiredLevel`,`InventoryType`,`OverallQualityID`,`AmmunitionType`,`VerifiedBuild`) VALUES
(1263,'','','','','Brain Hacker',0,0.4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3636,3181,0,0,0,0,0,0,0,0,7,32,-1,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,79064,395323,1,1,1,0,16384,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3500,0,0,0,60,-1,0,0,0,0,0,0,1,1,0,2,0,0,0,0,55,17,4,0,70862),
(12939,'','','','','Dal''Rend''s Tribal Guardian',0,0.6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4667,66666,0,0,0,0,0,0,0,0,12,50,-1,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,60363,301818,1,1,1,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,41,0,0,1800,0,0,0,63,-1,0,0,0,0,0,0,3,1,0,1,0,0,0,0,58,22,3,0,70862),
(12940,'','','','','Dal''Rend''s Sacred Charge',0,0.6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2667,9333,0,0,0,0,0,0,0,0,4,32,-1,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,54812,274063,1,1,1,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,41,0,0,2800,0,0,0,63,-1,0,0,0,0,0,0,3,1,0,1,0,0,0,0,58,21,3,0,70862),
(12965,'','','','','Spiritshroud Leggings',0,0.2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4444,4444,3611,5278,0,0,0,0,0,0,6,5,7,45,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,22344,111721,1,1,1,0,4,0,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,63,-1,0,0,0,0,0,0,0,7,0,1,0,0,0,0,58,7,3,0,70862),
(13142,'','','','','Brigam Girdle',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5185,6666,3704,0,0,0,0,0,0,0,4,7,31,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,11642,58210,1,1,1,0,4,0,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,63,-1,0,0,0,0,0,0,0,6,0,1,0,0,0,0,58,6,3,0,70862),
(18298,'','','','','Unbridled Leggings',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5625,3438,4687,2812,0,0,0,0,0,0,7,4,46,12,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,20685,103427,1,1,1,0,4,0,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,56,-1,0,0,0,0,0,0,0,8,0,2,0,0,0,0,51,7,3,0,70862),
(18680,'','','','','Ancient Bone Bow',0,0.6,0,0,0,0,0,0,100,0,0,0,0,0,0,0,0,0,0,10000,0,0,0,0,0,0,0,0,0,3,-1,-1,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,37894,189474,1,1,1,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2800,0,0,0,61,-1,0,0,0,0,0,0,0,2,0,1,0,0,0,0,56,15,3,2,70862),
(18699,'','','','','Icy Tomb Spaulders',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4583,4166,4167,4167,0,0,0,0,0,0,3,7,52,54,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,15380,76901,1,1,1,0,4,0,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,57,-1,0,0,0,0,0,0,0,8,0,2,0,0,0,0,52,3,3,0,70862),
(22247,'','','','','Faith Healer''s Boots',0,0.2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4444,4444,3704,8888,2962,0,0,0,0,0,6,5,7,41,42,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,17874,89371,1,1,1,0,4,0,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,63,-1,0,0,0,0,0,0,0,7,0,1,0,0,0,0,58,8,3,0,70862),
(280083,'','','','','Rifle of Insight',0,0.6,0,0,0,0,0,0,100,0,0,0,0,0,0,0,0,0,0,6666,8000,4444,0,0,0,0,0,0,0,32,38,7,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,37153,185768,1,1,0.9959,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2800,0,0,0,57,-1,0,0,0,0,0,0,0,1,0,1,0,0,0,0,52,26,3,3,70862);
DELETE FROM `hotfix_data` WHERE `Id` BETWEEN 2060000 AND 2060999;
DELETE FROM `hotfix_data` WHERE (`TableHash`,`RecordId`) IN ((1344507586,1263),(2442913102,1263),(1344507586,12939),(2442913102,12939),(1344507586,12940),(2442913102,12940),(1344507586,12965),(2442913102,12965),(1344507586,13142),(2442913102,13142),(1344507586,18298),(2442913102,18298),(1344507586,18680),(2442913102,18680),(1344507586,18699),(2442913102,18699),(1344507586,22247),(2442913102,22247),(1344507586,280083),(2442913102,280083));
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(2060000,1397780000,1344507586,1263,1,70862),
(2060001,1397780001,2442913102,1263,1,70862),
(2060002,1397780002,1344507586,12939,1,70862),
(2060003,1397780003,2442913102,12939,1,70862),
(2060004,1397780004,1344507586,12940,1,70862),
(2060005,1397780005,2442913102,12940,1,70862),
(2060006,1397780006,1344507586,12965,1,70862),
(2060007,1397780007,2442913102,12965,1,70862),
(2060008,1397780008,1344507586,13142,1,70862),
(2060009,1397780009,2442913102,13142,1,70862),
(2060010,1397780010,1344507586,18298,1,70862),
(2060011,1397780011,2442913102,18298,1,70862),
(2060012,1397780012,1344507586,18680,1,70862),
(2060013,1397780013,2442913102,18680,1,70862),
(2060014,1397780014,1344507586,18699,1,70862),
(2060015,1397780015,2442913102,18699,1,70862),
(2060016,1397780016,1344507586,22247,1,70862),
(2060017,1397780017,2442913102,22247,1,70862),
(2060018,1397780018,1344507586,280083,1,70862),
(2060019,1397780019,2442913102,280083,1,70862);
DELETE FROM `item_sparse_locale` WHERE `locale`='ruRU' AND `ID` IN (1263,12939,12940,12965,13142,18298,18680,18699,22247,280083);
INSERT INTO `item_sparse_locale` (`ID`,`locale`,`Description_lang`,`Display3_lang`,`Display2_lang`,`Display1_lang`,`Display_lang`,`VerifiedBuild`) VALUES
(1263,'ruRU','','','','','Мозгоруб',70862),
(12939,'ruRU','','','','','Племенной страж Дал''Ренда',70862),
(12940,'ruRU','','','','','Священный заряд Дал''Ренда',70862),
(12965,'ruRU','','','','','Поножи Покрова духов',70862),
(13142,'ruRU','','','','','Ремень Бригама',70862),
(18298,'ruRU','','','','','Невоздержанные поножи',70862),
(18680,'ruRU','','','','','Древний костяной лук',70862),
(18699,'ruRU','','','','','Наплеч ледяной могилы',70862),
(22247,'ruRU','','','','','Сапоги целителя Веры',70862),
(280083,'ruRU','','','','','Ружье проницательности',70862);
