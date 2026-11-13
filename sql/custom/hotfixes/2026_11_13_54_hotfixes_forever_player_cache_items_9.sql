-- WoW Forever: 4 items that still had no name or stats in our data, seen in the 2026-11-13 cache folder upload
-- ("cache_enUS_20261113.rar", two Cache folders; Cache\ADB\enUS\DBCache.bin, clients 1.60.1.70888 and 70938, Blizzard's live
-- realm): The Lion Horn of Stormwind, Malgen's Long Bow, Scales of the Devourer, Silithid Queen's Husk. Item + ItemSparse records
-- as Blizzard sends them (the last record of each in the caches). The caches hold no live push after 114051; their other new
-- records are items and NPC lines we already have. Made with tools/full_forever/live_hotfix_sql.py. hotfix_data ids 2090000-2090999. Safe to run again.

DELETE FROM `item` WHERE `ID` IN (14557,22318,282027,286590);
INSERT INTO `item` (`ID`,`ClassID`,`SubclassID`,`Material`,`InventoryType`,`SheatheType`,`ItemPetFoodID`,`SoundOverrideSubclassID`,`IconFileDataID`,`ItemGroupSoundsID`,`ContentTuningID`,`ModifiedCraftingReagentItemID`,`Unknown1200`,`CraftingQualityID`,`ItemSquishEraID`,`RecraftReagentCountPercentage`,`OrderSource`,`VerifiedBuild`) VALUES
(14557,4,0,4,12,0,0,-1,134229,12,0,0,0,0,0,0,0,70888),
(22318,2,2,2,15,0,0,-1,135500,13,0,0,2,0,0,0,0,70888),
(282027,4,3,5,5,0,0,-1,132628,10,0,0,0,0,0,0,0,70888),
(286590,4,4,6,5,0,0,-1,0,11,0,0,0,0,0,0,0,70888);
DELETE FROM `item_sparse` WHERE `ID` IN (14557,22318,282027,286590);
INSERT INTO `item_sparse` (`ID`,`Description`,`Display3`,`Display2`,`Display1`,`Display`,`ExpansionID`,`DmgVariance`,`LimitCategory`,`DurationInInventory`,`QualityModifier`,`BagFamily`,`StartQuestID`,`LanguageID`,`ItemRange`,`StatPercentageOfSocket1`,`StatPercentageOfSocket2`,`StatPercentageOfSocket3`,`StatPercentageOfSocket4`,`StatPercentageOfSocket5`,`StatPercentageOfSocket6`,`StatPercentageOfSocket7`,`StatPercentageOfSocket8`,`StatPercentageOfSocket9`,`StatPercentageOfSocket10`,`StatPercentEditor1`,`StatPercentEditor2`,`StatPercentEditor3`,`StatPercentEditor4`,`StatPercentEditor5`,`StatPercentEditor6`,`StatPercentEditor7`,`StatPercentEditor8`,`StatPercentEditor9`,`StatPercentEditor10`,`StatModifierBonusStat1`,`StatModifierBonusStat2`,`StatModifierBonusStat3`,`StatModifierBonusStat4`,`StatModifierBonusStat5`,`StatModifierBonusStat6`,`StatModifierBonusStat7`,`StatModifierBonusStat8`,`StatModifierBonusStat9`,`StatModifierBonusStat10`,`Stackable`,`MaxCount`,`MinReputation`,`RequiredAbility`,`AllowableRace1`,`AllowableRace2`,`SellPrice`,`BuyPrice`,`VendorStackCount`,`PriceVariance`,`PriceRandomValue`,`Flags1`,`Flags2`,`Flags3`,`Flags4`,`Flags5`,`FactionRelated`,`ModifiedCraftingReagentItemID`,`ContentTuningID`,`PlayerLevelToItemLevelCurveID`,`ItemLevelOffsetCurveID`,`ItemLevelOffsetItemLevel`,`ItemSquishEraID`,`ItemNameDescriptionID`,`RequiredTransmogHoliday`,`RequiredHoliday`,`GemProperties`,`SocketMatchEnchantmentId`,`TotemCategoryID`,`InstanceBound`,`ZoneBound1`,`ZoneBound2`,`ItemSet`,`LockID`,`PageID`,`ItemDelay`,`MinFactionID`,`RequiredSkillRank`,`RequiredSkill`,`ItemLevel`,`AllowableClass`,`ArtifactID`,`SpellWeight`,`SpellWeightCategory`,`SocketType1`,`SocketType2`,`SocketType3`,`SheatheType`,`Material`,`PageMaterialID`,`Bonding`,`DamageDamageType`,`ContainerSlots`,`RequiredPVPMedal`,`RequiredPVPRank`,`RequiredLevel`,`InventoryType`,`OverallQualityID`,`AmmunitionType`,`VerifiedBuild`) VALUES
(14557,'','','','','The Lion Horn of Stormwind',0,0,0,0,0,1048576,0,0,0,0,0,0,0,0,0,0,0,0,0,2352,0,0,0,0,0,0,0,0,0,12,-1,-1,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,17880,71520,1,1,1,524288,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,63,-1,0,0,0,0,0,0,0,4,0,2,0,0,0,0,58,12,4,0,70888),
(22318,'','','','','Malgen''s Long Bow',0,0.6,0,0,0,0,0,0,100,0,0,0,0,0,0,0,0,0,0,3636,16363,0,0,0,0,0,0,0,0,7,38,-1,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,37362,186810,1,1,1,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2900,0,0,0,61,-1,0,0,0,0,0,0,0,2,0,1,0,0,0,0,56,15,3,2,70888),
(282027,'','','','','Scales of the Devourer',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4182,5113,3900,4550,0,0,0,0,0,0,4,7,3,45,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,1001,5009,1,1,1.0333,0,4,0,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,62,-1,0,0,0,0,0,0,0,5,0,1,0,0,0,0,57,5,3,0,70888),
(286590,'The swarm can''t be satiated.','','','','Silithid Queen''s Husk',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8364,6667,3899,6818,0,0,0,0,0,0,125,7,32,38,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,5290,26452,1,1,0.9969,0,4,0,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,61,-1,0,0,0,0,0,0,0,6,0,1,0,0,0,0,56,5,3,0,70888);
DELETE FROM `hotfix_data` WHERE `Id` BETWEEN 2090000 AND 2090999;
DELETE FROM `hotfix_data` WHERE (`TableHash`,`RecordId`) IN ((1344507586,14557),(1344507586,22318),(1344507586,282027),(1344507586,286590),(2442913102,14557),(2442913102,22318),(2442913102,282027),(2442913102,286590));
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(2090000,1397810000,1344507586,14557,1,70888),
(2090001,1397810001,1344507586,22318,1,70888),
(2090002,1397810002,1344507586,282027,1,70888),
(2090003,1397810003,1344507586,286590,1,70888),
(2090004,1397810004,2442913102,14557,1,70888),
(2090005,1397810005,2442913102,22318,1,70888),
(2090006,1397810006,2442913102,282027,1,70888),
(2090007,1397810007,2442913102,286590,1,70888);
