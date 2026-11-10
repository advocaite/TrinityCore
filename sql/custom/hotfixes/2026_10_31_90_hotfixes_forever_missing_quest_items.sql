-- Forever 1.60: item_sparse rows for four quest items the client knows in Item.db2 but lacks in ItemSparse.db2
-- (checked against the 70532 and 70685 client data). Without them the items cannot exist, so these quests can't be done:
--   Seasoned Wolf Kabobs 90 (Stormwind Seasoning Herbs 2665, vendor item)
--   The Essence of Eranikus 3373 / 3374 (Essence of Eranikus 10454 starts 3373, Chained Essence of Eranikus 10455 is the reward)
--   Breastplate of the Chromatic Flight 5166 / Legplates of the Chromatic Defier 5167 (Chromatic Carapace 12871)
-- FOR THE HOTFIXES DATABASE (not world). Each row copies the layout of a similar item from the client's own ItemSparse
-- (Salt 4289, Shattered Necklace 7666, Nifty Stopwatch 2820, Reins of the Winterspring Frostsaber 13086 with its
-- Alliance-only, mount and race fields cleared) and sets name,
-- quality, prices, item level, required level, stack, bonding, start quest and flags from the 1.12 values.
-- 1.12 values read from CMaNGOS classic-db (cmangos/classic-db, ClassicDB z2815); please confirm them against the vMaNGOS
-- mangos.sql on the PC before shipping (GPL-2 source of record).
-- The Item.db2 rows (class, icon, material) and the trinket's use effect (ItemEffect spell 12766) are already in the client.
-- INSERT IGNORE: if the hotfixes DB already holds any of these items, the existing row wins (then the item was never the
-- cause, and that quest needs another look). Safe to run twice.

INSERT IGNORE INTO `item_sparse` (`ID`,`Description`,`Display3`,`Display2`,`Display1`,`Display`,`ExpansionID`,`DmgVariance`,`LimitCategory`,`DurationInInventory`,`QualityModifier`,`BagFamily`,`StartQuestID`,`LanguageID`,`ItemRange`,`StatPercentageOfSocket1`,`StatPercentageOfSocket2`,`StatPercentageOfSocket3`,`StatPercentageOfSocket4`,`StatPercentageOfSocket5`,`StatPercentageOfSocket6`,`StatPercentageOfSocket7`,`StatPercentageOfSocket8`,`StatPercentageOfSocket9`,`StatPercentageOfSocket10`,`StatPercentEditor1`,`StatPercentEditor2`,`StatPercentEditor3`,`StatPercentEditor4`,`StatPercentEditor5`,`StatPercentEditor6`,`StatPercentEditor7`,`StatPercentEditor8`,`StatPercentEditor9`,`StatPercentEditor10`,`StatModifierBonusStat1`,`StatModifierBonusStat2`,`StatModifierBonusStat3`,`StatModifierBonusStat4`,`StatModifierBonusStat5`,`StatModifierBonusStat6`,`StatModifierBonusStat7`,`StatModifierBonusStat8`,`StatModifierBonusStat9`,`StatModifierBonusStat10`,`Stackable`,`MaxCount`,`MinReputation`,`RequiredAbility`,`AllowableRace1`,`AllowableRace2`,`SellPrice`,`BuyPrice`,`VendorStackCount`,`PriceVariance`,`PriceRandomValue`,`Flags1`,`Flags2`,`Flags3`,`Flags4`,`Flags5`,`FactionRelated`,`ModifiedCraftingReagentItemID`,`ContentTuningID`,`PlayerLevelToItemLevelCurveID`,`ItemLevelOffsetCurveID`,`ItemLevelOffsetItemLevel`,`ItemSquishEraID`,`ItemNameDescriptionID`,`RequiredTransmogHoliday`,`RequiredHoliday`,`GemProperties`,`SocketMatchEnchantmentId`,`TotemCategoryID`,`InstanceBound`,`ZoneBound1`,`ZoneBound2`,`ItemSet`,`LockID`,`PageID`,`ItemDelay`,`MinFactionID`,`RequiredSkillRank`,`RequiredSkill`,`ItemLevel`,`AllowableClass`,`ArtifactID`,`SpellWeight`,`SpellWeightCategory`,`SocketType1`,`SocketType2`,`SocketType3`,`SheatheType`,`Material`,`PageMaterialID`,`Bonding`,`DamageDamageType`,`ContainerSlots`,`RequiredPVPMedal`,`RequiredPVPRank`,`RequiredLevel`,`InventoryType`,`OverallQualityID`,`AmmunitionType`,`VerifiedBuild`) VALUES
(2665,'','','','','Stormwind Seasoning Herbs',0,0.0,0,0,0.0,33554440,0,0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0,0,0,0,0,0,0,0,0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,20,0,0,0,-1,-1,5,20,1,1.0,1.0,0,-2147459068,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,-1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,70685),
(10454,'Green mists swirl inside the lattices of this gem.','','','','Essence of Eranikus',0,0.0,0,0,0.0,0,3373,0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0,0,0,0,0,0,0,0,0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,1,1,0,0,-1,-1,0,0,1,1.0,1.0,2048,24580,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,48,-1,0,0,0,0,0,0,0,0,0,1,0,0,0,0,48,0,2,0,70685),
(10455,'','','','','Chained Essence of Eranikus',0,0.2,0,0,0.0,1048576,0,0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0,0,0,0,0,0,0,0,0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,6464,25859,1,1.0,1.0,524288,8196,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,60,-1,0,0,0,0,0,0,0,4,0,1,0,0,0,0,0,12,2,0,70685),
(12871,'','','','','Chromatic Carapace',0,0.0,0,0,0.0,0,0,0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0,0,0,0,0,0,0,0,0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,8048,32195,1,1.0,1.0,0,24580,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,-1,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,4,0,70685);

INSERT IGNORE INTO `item_search_name` (`ID`,`Display`,`OverallQualityID`,`ExpansionID`,`MinFactionID`,`MinReputation`,`AllowableClass`,`RequiredLevel`,`RequiredSkill`,`RequiredSkillRank`,`RequiredAbility`,`ItemLevel`,`Flags1`,`Flags2`,`Flags3`,`Flags4`,`Flags5`,`AllowableRace1`,`AllowableRace2`,`VerifiedBuild`) VALUES
(2665,'Stormwind Seasoning Herbs',1,0,0,0,-1,0,0,0,0,1,0,-2147459068,0,0,0,-1,-1,70685),
(10454,'Essence of Eranikus',2,0,0,0,-1,48,0,0,0,48,2048,24580,0,0,0,-1,-1,70685),
(10455,'Chained Essence of Eranikus',2,0,0,0,-1,0,0,0,0,60,524288,8196,0,0,0,-1,-1,70685),
(12871,'Chromatic Carapace',4,0,0,0,-1,0,0,0,0,1,0,24580,0,0,0,-1,-1,70685);

-- hotfix_data so the client is sent the new records (ItemSparse 0x919BE54E, ItemSearchName 0x198E28B5). Ids 26103101-26103108:
-- check they are free first: SELECT MAX(Id) FROM hotfix_data;  (our highest seen in the repos is 9900003)
DELETE FROM `hotfix_data` WHERE `Id` BETWEEN 26103101 AND 26103108;
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(26103101,26103101,2442913102,2665,1,70685),
(26103102,26103102,2442913102,10454,1,70685),
(26103103,26103103,2442913102,10455,1,70685),
(26103104,26103104,2442913102,12871,1,70685),
(26103105,26103105,428746933,2665,1,70685),
(26103106,26103106,428746933,10454,1,70685),
(26103107,26103107,428746933,10455,1,70685),
(26103108,26103108,428746933,12871,1,70685);
