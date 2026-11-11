-- WoW Forever: real English names and flavor text for 11 items we had only translated from Russian caches. An English player
-- cache sent on 2026-11-10 (Cache\ADB\enUS\DBCache.bin, build 70862, Blizzard's live realm) has their records as Blizzard sends
-- them: 227935 Hammer of the Fallen Thane, 276455 Swordsmith Mitts, 280029 Cinderedge Crescent, 282085 Dragonblood Greatsword,
-- 284162 Mossflayer Witchdoctor's Robe, 284231 Sayaad Charm, 284587 Ankle Slicers, 285066 Ooze Rummagers, 287022 Spaulders of
-- Containment, plus the real flavor text of 284086 Scarlet Executioner's Guise and 22253 Tome of the Lost. Replaces the rows
-- from 2026_11_08_64 and 2026_11_10_40 for these items; their Russian text in item_sparse_locale stays.
-- Made with tools/full_forever/live_hotfix_sql.py. hotfix_data ids 2059000-2059999. Safe to run again.

DELETE FROM `item` WHERE `ID` IN (22253,227935,276455,280029,282085,284086,284162,284231,284587,285066,287022);
INSERT INTO `item` (`ID`,`ClassID`,`SubclassID`,`Material`,`InventoryType`,`SheatheType`,`ItemPetFoodID`,`SoundOverrideSubclassID`,`IconFileDataID`,`ItemGroupSoundsID`,`ContentTuningID`,`ModifiedCraftingReagentItemID`,`Unknown1200`,`CraftingQualityID`,`ItemSquishEraID`,`RecraftReagentCountPercentage`,`OrderSource`,`VerifiedBuild`) VALUES
(22253,4,0,7,23,3,0,-1,133738,19,0,0,0,0,0,0,0,70862),
(227935,2,4,2,21,3,0,-1,133720,8,0,0,0,0,0,0,0,70862),
(276455,4,2,8,10,0,0,-1,132938,7,0,0,0,0,0,0,0,70862),
(280029,2,0,1,13,3,0,-1,0,8,0,0,0,0,0,0,0,70862),
(282085,2,8,1,17,1,0,-1,135329,9,0,0,0,0,0,0,0,70862),
(284086,4,2,8,1,0,0,-1,0,7,0,0,0,0,0,0,0,70862),
(284162,4,1,7,5,0,0,-1,132642,7,0,0,0,0,0,0,0,70862),
(284231,4,0,4,12,0,0,-1,136129,8,0,0,0,0,0,0,0,70862),
(284587,2,16,1,25,0,0,-1,135431,8,0,0,0,0,0,0,0,70862),
(285066,4,1,7,10,0,0,-1,0,10,0,0,0,0,0,0,0,70862),
(287022,4,3,5,3,0,0,-1,135053,10,0,0,0,0,0,0,0,70862);
DELETE FROM `item_sparse` WHERE `ID` IN (22253,227935,276455,280029,282085,284086,284162,284231,284587,285066,287022);
INSERT INTO `item_sparse` (`ID`,`Description`,`Display3`,`Display2`,`Display1`,`Display`,`ExpansionID`,`DmgVariance`,`LimitCategory`,`DurationInInventory`,`QualityModifier`,`BagFamily`,`StartQuestID`,`LanguageID`,`ItemRange`,`StatPercentageOfSocket1`,`StatPercentageOfSocket2`,`StatPercentageOfSocket3`,`StatPercentageOfSocket4`,`StatPercentageOfSocket5`,`StatPercentageOfSocket6`,`StatPercentageOfSocket7`,`StatPercentageOfSocket8`,`StatPercentageOfSocket9`,`StatPercentageOfSocket10`,`StatPercentEditor1`,`StatPercentEditor2`,`StatPercentEditor3`,`StatPercentEditor4`,`StatPercentEditor5`,`StatPercentEditor6`,`StatPercentEditor7`,`StatPercentEditor8`,`StatPercentEditor9`,`StatPercentEditor10`,`StatModifierBonusStat1`,`StatModifierBonusStat2`,`StatModifierBonusStat3`,`StatModifierBonusStat4`,`StatModifierBonusStat5`,`StatModifierBonusStat6`,`StatModifierBonusStat7`,`StatModifierBonusStat8`,`StatModifierBonusStat9`,`StatModifierBonusStat10`,`Stackable`,`MaxCount`,`MinReputation`,`RequiredAbility`,`AllowableRace1`,`AllowableRace2`,`SellPrice`,`BuyPrice`,`VendorStackCount`,`PriceVariance`,`PriceRandomValue`,`Flags1`,`Flags2`,`Flags3`,`Flags4`,`Flags5`,`FactionRelated`,`ModifiedCraftingReagentItemID`,`ContentTuningID`,`PlayerLevelToItemLevelCurveID`,`ItemLevelOffsetCurveID`,`ItemLevelOffsetItemLevel`,`ItemSquishEraID`,`ItemNameDescriptionID`,`RequiredTransmogHoliday`,`RequiredHoliday`,`GemProperties`,`SocketMatchEnchantmentId`,`TotemCategoryID`,`InstanceBound`,`ZoneBound1`,`ZoneBound2`,`ItemSet`,`LockID`,`PageID`,`ItemDelay`,`MinFactionID`,`RequiredSkillRank`,`RequiredSkill`,`ItemLevel`,`AllowableClass`,`ArtifactID`,`SpellWeight`,`SpellWeightCategory`,`SocketType1`,`SocketType2`,`SocketType3`,`SheatheType`,`Material`,`PageMaterialID`,`Bonding`,`DamageDamageType`,`ContainerSlots`,`RequiredPVPMedal`,`RequiredPVPRank`,`RequiredLevel`,`InventoryType`,`OverallQualityID`,`AmmunitionType`,`VerifiedBuild`) VALUES
(22253,'The title reads "The Brode to Nowhere"','','','','Tome of the Lost',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3500,4000,9000,0,0,0,0,0,0,0,5,7,45,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,34450,137801,1,1,1,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,63,-1,0,0,0,0,0,0,3,7,0,1,0,0,0,0,58,23,3,0,70862),
(227935,'','','','','Hammer of the Fallen Thane',0,0.6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3611,7150,1629,0,0,0,0,0,0,0,7,5,43,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,12042,60211,1,1,1.0445,0,4,0,0,768,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2200,0,0,0,59,-1,0,0,0,0,0,0,3,2,0,1,0,0,0,0,0,21,3,0,70862),
(276455,'','','','','Swordsmith Mitts',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3846,1153,3076,0,0,0,0,0,0,0,51,47,5,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,3334,16673,1,1,1.0254,0,4,0,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,60,-1,0,0,0,0,0,0,0,8,0,1,0,0,0,0,55,10,3,0,70862),
(280029,'','','','','Cinderedge Crescent',0,0.6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6666,9333,4444,0,0,0,0,0,0,0,7,38,32,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,50652,253261,1,1,1.0183,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2400,0,0,0,63,-1,0,0,0,0,0,0,3,1,0,1,0,0,0,0,58,13,3,0,70862),
(282085,'Bathed in the blood of the great drake Hematos.','','','','Dragonblood Greatsword',0,0.4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7150,3900,11700,0,0,0,0,0,0,0,7,4,129,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,2854,14273,1,1,0.9519,0,16388,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3500,0,0,0,62,-1,0,0,0,0,0,0,1,1,0,1,0,0,0,0,57,17,3,0,70862),
(284086,'Anyone the Scarlet Crusade justified killing saw only empty eyes before their death.','','','','Scarlet Executioner''s Guise',0,0.2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6667,4114,4641,0,0,0,0,0,0,0,4,31,6,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,2699,13498,1,1,1.0044,0,4,0,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,62,-1,0,0,0,0,0,0,0,8,0,1,0,0,0,0,57,1,3,0,70862),
(284162,'','','','','Mossflayer Witchdoctor''s Robe',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2964,10715,1778,0,0,0,0,0,0,0,5,46,43,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,648,3242,1,1,0.9892,0,4,0,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,61,-1,0,0,0,0,0,0,0,7,0,2,0,0,0,0,56,5,2,0,70862),
(284231,'Not quite as seductive as a Sayaad.','','','','Sayaad Charm',0,0,0,0,0,1048576,0,0,0,0,0,0,0,0,0,0,0,0,0,7407,0,0,0,0,0,0,0,0,0,54,-1,-1,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,16250,65000,1,1,0.9747,524288,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,63,-1,0,0,0,0,0,0,0,4,0,1,0,0,0,0,58,12,3,0,70862),
(284587,'','','','','Ankle Slicers',0,0.6,0,0,0,0,0,0,100,0,0,0,0,0,0,0,0,0,0,6428,8571,0,0,0,0,0,0,0,0,7,38,-1,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,57582,287913,1,1,0.9636,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1800,0,0,0,63,-1,0,0,0,0,0,0,0,1,0,2,0,0,0,0,58,25,4,0,70862),
(285066,'','','','','Ooze Rummagers',0,0.2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6667,6666,0,0,0,0,0,0,0,0,7,6,-1,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,10801,54007,1,1,1.0157,0,4,0,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,59,-1,0,0,0,0,0,0,0,7,0,1,0,0,0,0,54,10,3,0,70862),
(287022,'','','','','Spaulders of Containment',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6667,4444,4444,0,0,0,0,0,0,0,7,4,37,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,0,0,1,1,0.997,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,52,-1,0,0,0,0,0,0,0,5,0,2,0,0,0,0,47,3,3,0,70862);
DELETE FROM `hotfix_data` WHERE `Id` BETWEEN 2059000 AND 2059999;
DELETE FROM `hotfix_data` WHERE (`TableHash`,`RecordId`) IN ((1344507586,22253),(1344507586,227935),(1344507586,276455),(1344507586,280029),(1344507586,282085),(1344507586,284086),(1344507586,284162),(1344507586,284231),(1344507586,284587),(1344507586,285066),(1344507586,287022),(2442913102,22253),(2442913102,227935),(2442913102,276455),(2442913102,280029),(2442913102,282085),(2442913102,284086),(2442913102,284162),(2442913102,284231),(2442913102,284587),(2442913102,285066),(2442913102,287022));
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(2059000,1397779000,1344507586,22253,1,70862),
(2059001,1397779001,1344507586,227935,1,70862),
(2059002,1397779002,1344507586,276455,1,70862),
(2059003,1397779003,1344507586,280029,1,70862),
(2059004,1397779004,1344507586,282085,1,70862),
(2059005,1397779005,1344507586,284086,1,70862),
(2059006,1397779006,1344507586,284162,1,70862),
(2059007,1397779007,1344507586,284231,1,70862),
(2059008,1397779008,1344507586,284587,1,70862),
(2059009,1397779009,1344507586,285066,1,70862),
(2059010,1397779010,1344507586,287022,1,70862),
(2059011,1397779011,2442913102,22253,1,70862),
(2059012,1397779012,2442913102,227935,1,70862),
(2059013,1397779013,2442913102,276455,1,70862),
(2059014,1397779014,2442913102,280029,1,70862),
(2059015,1397779015,2442913102,282085,1,70862),
(2059016,1397779016,2442913102,284086,1,70862),
(2059017,1397779017,2442913102,284162,1,70862),
(2059018,1397779018,2442913102,284231,1,70862),
(2059019,1397779019,2442913102,284587,1,70862),
(2059020,1397779020,2442913102,285066,1,70862),
(2059021,1397779021,2442913102,287022,1,70862);
