-- WoW Forever: 13 items that still had no name or stats in our data, seen in a Russian player's cache folder ("Cache 4.rar",
-- Cache\ADB\ruRU\DBCache.bin, client 1.60.1.70888, Blizzard's live realm, written 2026-11-12 00:27): Dustfeather Sash, Felstriker,
-- Polychromatic Visionwrap, Handcrafted Mastersmith Girdle, Runeblade of Baron Rivendare, Juno's Shadow, Mastersmith's Hammer,
-- Soulwoven Drape, Arcanum of Binding, Scrawled Note, Predaceous Band, Predator's Shroud, Ancient Mattock.
-- Item + ItemSparse records with English text from the public mirror of the live hotfix file
-- (https://storage.googleapis.com/raidbots-static/wow/forever/enUS/DBCache.bin, the same records), Russian text from the player's
-- cache in item_sparse_locale. Made with tools/full_forever/live_hotfix_sql.py. hotfix_data ids 2084000-2084999. Safe to run again.

DELETE FROM `item` WHERE `ID` IN (12589,12590,12609,13502,13505,17061,18048,276072,282707,283387,287043,287044,287045);
INSERT INTO `item` (`ID`,`ClassID`,`SubclassID`,`Material`,`InventoryType`,`SheatheType`,`ItemPetFoodID`,`SoundOverrideSubclassID`,`IconFileDataID`,`ItemGroupSoundsID`,`ContentTuningID`,`ModifiedCraftingReagentItemID`,`Unknown1200`,`CraftingQualityID`,`ItemSquishEraID`,`RecraftReagentCountPercentage`,`OrderSource`,`VerifiedBuild`) VALUES
(12589,4,1,7,6,0,0,-1,132500,7,0,0,0,0,0,0,0,70888),
(12590,2,15,1,13,3,0,-1,135661,8,0,0,0,0,0,0,0,70888),
(12609,4,1,8,5,0,0,-1,132658,7,0,0,0,0,0,0,0,70888),
(13502,4,4,6,6,0,0,-1,132512,11,0,0,0,0,0,0,0,70888),
(13505,2,8,1,17,1,0,-1,135315,9,0,0,0,0,0,0,0,70888),
(17061,4,1,7,16,0,0,-1,133772,7,0,0,0,0,0,0,0,70888),
(18048,2,4,2,21,3,0,-1,133043,8,0,0,0,0,0,0,0,70888),
(276072,4,1,7,16,0,0,-1,133772,7,0,0,0,0,0,0,0,70888),
(282707,12,0,0,0,0,0,-1,134072,24,0,0,0,0,0,0,0,70888),
(283387,15,4,0,0,0,0,-1,134331,7,0,0,0,0,0,0,0,70888),
(287043,4,0,3,11,0,0,-1,133356,14,0,0,0,0,0,0,0,70888),
(287044,4,1,7,16,0,0,-1,0,7,0,0,0,0,0,0,0,70888),
(287045,2,0,1,13,0,0,-1,0,8,0,0,0,0,0,0,0,70888);
DELETE FROM `item_sparse` WHERE `ID` IN (12589,12590,12609,13502,13505,17061,18048,276072,282707,283387,287043,287044,287045);
INSERT INTO `item_sparse` (`ID`,`Description`,`Display3`,`Display2`,`Display1`,`Display`,`ExpansionID`,`DmgVariance`,`LimitCategory`,`DurationInInventory`,`QualityModifier`,`BagFamily`,`StartQuestID`,`LanguageID`,`ItemRange`,`StatPercentageOfSocket1`,`StatPercentageOfSocket2`,`StatPercentageOfSocket3`,`StatPercentageOfSocket4`,`StatPercentageOfSocket5`,`StatPercentageOfSocket6`,`StatPercentageOfSocket7`,`StatPercentageOfSocket8`,`StatPercentageOfSocket9`,`StatPercentageOfSocket10`,`StatPercentEditor1`,`StatPercentEditor2`,`StatPercentEditor3`,`StatPercentEditor4`,`StatPercentEditor5`,`StatPercentEditor6`,`StatPercentEditor7`,`StatPercentEditor8`,`StatPercentEditor9`,`StatPercentEditor10`,`StatModifierBonusStat1`,`StatModifierBonusStat2`,`StatModifierBonusStat3`,`StatModifierBonusStat4`,`StatModifierBonusStat5`,`StatModifierBonusStat6`,`StatModifierBonusStat7`,`StatModifierBonusStat8`,`StatModifierBonusStat9`,`StatModifierBonusStat10`,`Stackable`,`MaxCount`,`MinReputation`,`RequiredAbility`,`AllowableRace1`,`AllowableRace2`,`SellPrice`,`BuyPrice`,`VendorStackCount`,`PriceVariance`,`PriceRandomValue`,`Flags1`,`Flags2`,`Flags3`,`Flags4`,`Flags5`,`FactionRelated`,`ModifiedCraftingReagentItemID`,`ContentTuningID`,`PlayerLevelToItemLevelCurveID`,`ItemLevelOffsetCurveID`,`ItemLevelOffsetItemLevel`,`ItemSquishEraID`,`ItemNameDescriptionID`,`RequiredTransmogHoliday`,`RequiredHoliday`,`GemProperties`,`SocketMatchEnchantmentId`,`TotemCategoryID`,`InstanceBound`,`ZoneBound1`,`ZoneBound2`,`ItemSet`,`LockID`,`PageID`,`ItemDelay`,`MinFactionID`,`RequiredSkillRank`,`RequiredSkill`,`ItemLevel`,`AllowableClass`,`ArtifactID`,`SpellWeight`,`SpellWeightCategory`,`SocketType1`,`SocketType2`,`SocketType3`,`SheatheType`,`Material`,`PageMaterialID`,`Bonding`,`DamageDamageType`,`ContainerSlots`,`RequiredPVPMedal`,`RequiredPVPRank`,`RequiredLevel`,`InventoryType`,`OverallQualityID`,`AmmunitionType`,`VerifiedBuild`) VALUES
(12589,'','','','','Dustfeather Sash',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6538,3846,3462,1153,0,0,0,0,0,0,5,7,45,47,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,10251,51257,1,1,1,0,4,0,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,61,-1,0,0,0,0,0,0,0,7,0,1,0,0,0,0,56,6,3,0,70888),
(12590,'','','','','Felstriker',0,0.6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,75624,378124,1,1,1,524288,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1700,0,0,0,63,-1,0,0,0,0,0,0,3,1,0,1,0,0,0,0,58,13,4,0,70888),
(12609,'','','','','Polychromatic Visionwrap',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4000,5714,0,0,0,0,0,0,0,0,7,124,-1,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,20498,102494,1,1,1,0,4,0,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,61,-1,0,0,0,0,0,0,0,8,0,1,0,0,0,0,56,5,3,0,70888),
(13502,'','','','','Handcrafted Mastersmith Girdle',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4074,3704,3704,55556,0,0,0,0,0,0,4,7,3,50,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,11987,59937,1,1,1,0,4,0,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,63,-1,0,0,0,0,0,0,0,6,0,1,0,0,0,0,58,6,3,0,70888),
(13505,'','','','','Runeblade of Baron Rivendare',0,0.4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,91345,456725,1,1,1,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3600,0,0,0,63,-1,0,0,0,0,0,0,1,1,0,1,0,0,0,0,58,17,4,0,70888),
(17061,'','','','','Juno''s Shadow',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4210,2632,7895,0,0,0,0,0,0,0,7,5,54,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,12888,64441,1,1,1,0,4,0,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,58,-1,0,0,0,0,0,0,0,7,0,2,0,0,0,0,53,16,3,0,70888),
(18048,'','','','','Mastersmith''s Hammer',0,0.6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6666,6666,0,0,0,0,0,0,0,0,31,7,-1,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,49204,246023,1,1,1,0,4,0,0,768,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2400,0,0,0,60,-1,0,0,0,0,0,0,3,2,0,1,0,0,0,0,55,21,3,0,70888),
(276072,'','','','','Soulwoven Drape',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6000,10000,3500,4000,0,0,0,0,0,0,6,41,42,7,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,1836,9181,1,1,0.9845,0,4,0,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,61,-1,0,0,0,0,0,0,0,7,0,1,0,0,0,0,56,16,3,0,70888),
(282707,'','','','','Arcanum of Binding',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,0,0,1,1,0.967,64,16388,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,50,-1,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,2,0,70888),
(283387,'','','','','Scrawled Note',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,0,0,1,1,0.9743,65536,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,10979,0,0,0,0,1,-1,0,0,0,0,0,0,0,0,1,1,0,0,0,0,1,0,1,0,70888),
(287043,'','','','','Predaceous Band',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7368,9471,0,0,0,0,0,0,0,0,7,131,-1,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,0,0,1,1,1.027,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,60,-1,0,0,0,0,0,0,0,3,0,2,0,0,0,0,55,11,3,0,70888),
(287044,'','','','','Predator''s Shroud',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7150,5748,4497,0,0,0,0,0,0,0,7,38,3,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,0,0,1,1,1.0307,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,60,-1,0,0,0,0,0,0,0,7,0,2,0,0,0,0,55,16,3,0,70888),
(287045,'','','','','Ancient Mattock',0,0.6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5000,5000,0,0,0,0,0,0,0,0,7,6,-1,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,0,0,1,1,1.0344,192,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2600,0,0,0,61,-1,0,0,0,0,0,0,0,1,0,2,0,0,0,0,56,13,3,0,70888);
DELETE FROM `hotfix_data` WHERE `Id` BETWEEN 2084000 AND 2084999;
DELETE FROM `hotfix_data` WHERE (`TableHash`,`RecordId`) IN ((1344507586,12589),(1344507586,12590),(1344507586,12609),(1344507586,13502),(1344507586,13505),(1344507586,17061),(1344507586,18048),(1344507586,276072),(1344507586,282707),(1344507586,283387),(1344507586,287043),(1344507586,287044),(1344507586,287045),(2442913102,12589),(2442913102,12590),(2442913102,12609),(2442913102,13502),(2442913102,13505),(2442913102,17061),(2442913102,18048),(2442913102,276072),(2442913102,282707),(2442913102,283387),(2442913102,287043),(2442913102,287044),(2442913102,287045));
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(2084000,1397804000,1344507586,12589,1,70888),
(2084001,1397804001,1344507586,12590,1,70888),
(2084002,1397804002,1344507586,12609,1,70888),
(2084003,1397804003,1344507586,13502,1,70888),
(2084004,1397804004,1344507586,13505,1,70888),
(2084005,1397804005,1344507586,17061,1,70888),
(2084006,1397804006,1344507586,18048,1,70888),
(2084007,1397804007,1344507586,276072,1,70888),
(2084008,1397804008,1344507586,282707,1,70888),
(2084009,1397804009,1344507586,283387,1,70888),
(2084010,1397804010,1344507586,287043,1,70888),
(2084011,1397804011,1344507586,287044,1,70888),
(2084012,1397804012,1344507586,287045,1,70888),
(2084013,1397804013,2442913102,12589,1,70888),
(2084014,1397804014,2442913102,12590,1,70888),
(2084015,1397804015,2442913102,12609,1,70888),
(2084016,1397804016,2442913102,13502,1,70888),
(2084017,1397804017,2442913102,13505,1,70888),
(2084018,1397804018,2442913102,17061,1,70888),
(2084019,1397804019,2442913102,18048,1,70888),
(2084020,1397804020,2442913102,276072,1,70888),
(2084021,1397804021,2442913102,282707,1,70888),
(2084022,1397804022,2442913102,283387,1,70888),
(2084023,1397804023,2442913102,287043,1,70888),
(2084024,1397804024,2442913102,287044,1,70888),
(2084025,1397804025,2442913102,287045,1,70888);

-- Russian texts (from the player's cache).
DELETE FROM `item_sparse_locale` WHERE `locale`='ruRU' AND `ID` IN (12589,12590,12609,13502,13505,17061,18048,276072,282707,283387,287043,287044,287045);
INSERT INTO `item_sparse_locale` (`ID`,`locale`,`Description_lang`,`Display3_lang`,`Display2_lang`,`Display1_lang`,`Display_lang`,`VerifiedBuild`) VALUES
(12589,'ruRU','','','','','Кушак пыльного пера',70888),
(12590,'ruRU','','','','','Оскверненный боец',70888),
(12609,'ruRU','','','','','Многоцветная блуза видения',70888),
(13502,'ruRU','','','','','Ремень ручной работы мастера-кузнеца',70888),
(13505,'ruRU','','','','','Рунический клинок барона Ривендера',70888),
(17061,'ruRU','','','','','Тень Джуно',70888),
(18048,'ruRU','','','','','Молот мастера-кузнеца',70888),
(276072,'ruRU','','','','','Духотканая пелерина',70888),
(282707,'ruRU','','','','','Магический знак сковывания',70888),
(283387,'ruRU','','','','','Нацарапанная записка',70888),
(287043,'ruRU','','','','','Перстень хищника',70888),
(287044,'ruRU','','','','','Покров хищника',70888),
(287045,'ruRU','','','','','Древнее кайло',70888);
DELETE FROM `spell_name_locale` WHERE `locale`='ruRU' AND `ID` IN (1277455);
INSERT INTO `spell_name_locale` (`ID`,`locale`,`Name_lang`,`VerifiedBuild`) VALUES
(1277455,'ruRU','Ошарашивающая вспышка',70888);
