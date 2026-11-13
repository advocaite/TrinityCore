-- Classic 1.60 (WoW Forever): Naxxramas items The Face of Death (23043, shield) and The Hungering Cold (23577, one-hand sword).
-- Both are in the client Item.db2 but not in its ItemSparse.db2, so the server had no such items: their loot rows (reference
-- 30466 and 30470) dropped nothing. Two item_sparse rows from the vMaNGOS 1.12 items, written by
-- tools/classic_items/gen_hotfix_items.py (with --shield-armor), sent to clients through hotfix_data:
--   23043: 3354 shield armor from the client table + 140 bonus armor = 3494, 21 stamina, 3% block as 15 block rating,
--          21 block value (shield block 66 is in item_classic_block), item level 90.
--   23577: 76-143 damage at 1.5 speed from the client table, 14 stamina, 140 bonus armor, +6 swords (spell 15763 as an
--          item_effect row), item level 89.
-- Check in game: ".additem 23043" and ".additem 23577" show the items with these stats. Safe to run again.
DELETE FROM `item_sparse` WHERE `ID` IN (23043,23577);
INSERT INTO `item_sparse` (`ID`,`Description`,`Display3`,`Display2`,`Display1`,`Display`,`ExpansionID`,`DmgVariance`,`LimitCategory`,`DurationInInventory`,`QualityModifier`,`BagFamily`,`StartQuestID`,`LanguageID`,`ItemRange`,`StatPercentageOfSocket1`,`StatPercentageOfSocket2`,`StatPercentageOfSocket3`,`StatPercentageOfSocket4`,`StatPercentageOfSocket5`,`StatPercentageOfSocket6`,`StatPercentageOfSocket7`,`StatPercentageOfSocket8`,`StatPercentageOfSocket9`,`StatPercentageOfSocket10`,`StatPercentEditor1`,`StatPercentEditor2`,`StatPercentEditor3`,`StatPercentEditor4`,`StatPercentEditor5`,`StatPercentEditor6`,`StatPercentEditor7`,`StatPercentEditor8`,`StatPercentEditor9`,`StatPercentEditor10`,`StatModifierBonusStat1`,`StatModifierBonusStat2`,`StatModifierBonusStat3`,`StatModifierBonusStat4`,`StatModifierBonusStat5`,`StatModifierBonusStat6`,`StatModifierBonusStat7`,`StatModifierBonusStat8`,`StatModifierBonusStat9`,`StatModifierBonusStat10`,`Stackable`,`MaxCount`,`MinReputation`,`RequiredAbility`,`AllowableRace1`,`AllowableRace2`,`SellPrice`,`BuyPrice`,`VendorStackCount`,`PriceVariance`,`PriceRandomValue`,`Flags1`,`Flags2`,`Flags3`,`Flags4`,`Flags5`,`FactionRelated`,`ModifiedCraftingReagentItemID`,`ContentTuningID`,`PlayerLevelToItemLevelCurveID`,`ItemLevelOffsetCurveID`,`ItemLevelOffsetItemLevel`,`ItemSquishEraID`,`ItemNameDescriptionID`,`RequiredTransmogHoliday`,`RequiredHoliday`,`GemProperties`,`SocketMatchEnchantmentId`,`TotemCategoryID`,`InstanceBound`,`ZoneBound1`,`ZoneBound2`,`ItemSet`,`LockID`,`PageID`,`ItemDelay`,`MinFactionID`,`RequiredSkillRank`,`RequiredSkill`,`ItemLevel`,`AllowableClass`,`ArtifactID`,`SpellWeight`,`SpellWeightCategory`,`SocketType1`,`SocketType2`,`SocketType3`,`SheatheType`,`Material`,`PageMaterialID`,`Bonding`,`DamageDamageType`,`ContainerSlots`,`RequiredPVPMedal`,`RequiredPVPRank`,`RequiredLevel`,`InventoryType`,`OverallQualityID`,`AmmunitionType`,`VerifiedBuild`) VALUES
(23043,'','','','','The Face of Death',0,0,0,0,0.0,0,0,0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,6176,41176,4412,6176,0,0,0,0,0,0,7,50,15,48,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,174810,874054,1,1.0,1.0,0,24580,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,90,-1,0,0,0,0,0,0,4,6,0,1,0,0,0,0,60,14,4,0,70938),
(23577,'','','','','The Hungering Cold',0,0.6,0,0,0.0,0,0,0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,5600,56000,0,0,0,0,0,0,0,0,7,50,-1,-1,-1,-1,-1,-1,-1,-1,1,1,0,0,-1,-1,269253,1346265,1,1.0,1.0,0,24580,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1500,0,0,0,89,-1,0,0,0,0,0,0,3,1,0,1,0,0,0,0,60,13,4,0,70938);

DELETE FROM `hotfix_data` WHERE `TableHash` = 2442913102 AND `RecordId` IN (23043,23577);
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(2088000,1397880000,2442913102,23043,1,70938),
(2088001,1397880001,2442913102,23577,1,70938);

DELETE FROM `item_effect` WHERE `ID` IN (9910101);
INSERT INTO `item_effect` (`ID`,`LegacySlotIndex`,`TriggerType`,`Charges`,`CoolDownMSec`,`CategoryCoolDownMSec`,`SpellCategoryID`,`SpellID`,`ChrSpecializationID`,`PlayerConditionID`,`VerifiedBuild`) VALUES
(9910101,0,1,0,-1,-1,0,15763,0,0,70938);
DELETE FROM `item_x_item_effect` WHERE `ID` IN (9910101);
INSERT INTO `item_x_item_effect` (`ID`,`ItemEffectID`,`ItemID`,`VerifiedBuild`) VALUES
(9910101,9910101,23577,70938);
DELETE FROM `hotfix_data` WHERE (`TableHash` = 1073915313 OR `TableHash` = 13330255) AND `RecordId` IN (9910101);
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(2088002,1397880002,1073915313,9910101,1,70938),
(2088003,1397880003,13330255,9910101,1,70938);
