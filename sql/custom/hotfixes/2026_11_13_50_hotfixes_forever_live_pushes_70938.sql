-- WoW Forever: Blizzard's live hotfix pushes 114016-114051 (client 1.60.1.70938), after 2026_11_12_52 (which stops at 114015).
-- Records from the public mirror of the live hotfix file
-- (https://storage.googleapis.com/raidbots-static/wow/forever/enUS/DBCache.bin, read 2026-11-13 02:10, build 70938). Blizzard's values, unchanged.
-- What changed live: new spell Dedicated Study (1225469, 23 h cooldown), Taran Icebreaker (2915), two spell effects of 437589,
-- quest bit flags for 10 quests (2768-3527), one creature display, 7 object displays, one item appearance, spell visuals.
-- Only tables the server reads; creature, quest POI, trade skill and other client-only rows are left out.
-- Made with tools/full_forever/live_hotfix_sql.py. hotfix_data ids 2086000-2086999. Safe to run again.

DELETE FROM `anim_kit` WHERE `ID` IN (39379);
INSERT INTO `anim_kit` (`ID`,`OneShotDuration`,`OneShotStopAnimKitID`,`LowDefAnimKitID`,`VerifiedBuild`) VALUES
(39379,0,0,0,70938);
DELETE FROM `creature_display_info` WHERE `ID` IN (149196);
INSERT INTO `creature_display_info` (`ID`,`ModelID`,`SoundID`,`SizeClass`,`CreatureModelScale`,`CreatureModelAlpha`,`BloodID`,`ExtendedDisplayInfoID`,`NPCSoundID`,`ParticleColorID`,`PortraitCreatureDisplayInfoID`,`PortraitTextureFileDataID`,`ObjectEffectPackageID`,`AnimReplacementSetID`,`Flags`,`StateSpellVisualKitID`,`PlayerOverrideScale`,`PetInstanceScale`,`UnarmedWeaponType`,`MountPoofSpellVisualKitID`,`DissolveEffectID`,`Gender`,`DissolveOutEffectID`,`CreatureModelMinLod`,`ConditionalCreatureModelID`,`Unknown_1100_1`,`Unknown_1100_2`,`TextureVariationFileDataID1`,`TextureVariationFileDataID2`,`TextureVariationFileDataID3`,`TextureVariationFileDataID4`,`VerifiedBuild`) VALUES
(149196,7599,0,1,1,255,0,166574,46,0,0,0,0,0,0,0,0,1,-1,0,0,1,0,0,0,45,0,0,0,0,0,70938);
DELETE FROM `gameobject_display_info` WHERE `ID` IN (135692,135693,135695,135696,135697,135698,135699);
INSERT INTO `gameobject_display_info` (`ID`,`GeoBoxMinX`,`GeoBoxMinY`,`GeoBoxMinZ`,`GeoBoxMaxX`,`GeoBoxMaxY`,`GeoBoxMaxZ`,`FileDataID`,`ObjectEffectPackageID`,`OverrideLootEffectScale`,`OverrideNameScale`,`AlternateDisplayType`,`ClientCreatureDisplayInfoID`,`ClientItemID`,`Unknown1100`,`VerifiedBuild`) VALUES
(135692,-1.899763,-1.436315,-0.2030461,2.271868,1.364247,3.821558,357810,0,0,0,1,140365,0,0,70938),
(135693,-5.25912,-2.99506,-1.71775,3.17733,3.4481,6.08871,125578,0,0,0,1,144460,0,0,70938),
(135695,-9.331356,-15.2485,-6.667162,9.331352,13.46025,10.00551,1261270,0,0,0,0,0,0,0,70938),
(135696,-6.101186,-5.605978,-3.934794,3.685176,5.647768,11.12693,6386604,0,0,0,0,0,0,0,70938),
(135697,-21.87965,-18.55837,-8.919806,22.68907,18.70205,26.47476,948417,0,0,0,0,0,0,0,70938),
(135698,-4.644574,-3.749104,-2.726525,4.543822,3.685832,8.001918,1733731,0,0,0,0,0,0,0,70938),
(135699,-4.64,-3.709,-1.46,4.986,3.619,8.084,126078,0,0,0,0,0,0,0,70938);
DELETE FROM `item` WHERE `ID` IN (2915);
INSERT INTO `item` (`ID`,`ClassID`,`SubclassID`,`Material`,`InventoryType`,`SheatheType`,`ItemPetFoodID`,`SoundOverrideSubclassID`,`IconFileDataID`,`ItemGroupSoundsID`,`ContentTuningID`,`ModifiedCraftingReagentItemID`,`Unknown1200`,`CraftingQualityID`,`ItemSquishEraID`,`RecraftReagentCountPercentage`,`OrderSource`,`VerifiedBuild`) VALUES
(2915,2,5,2,17,1,0,-1,133039,9,0,0,0,0,0,0,0,70938);
DELETE FROM `item_modified_appearance` WHERE `ID` IN (311402);
INSERT INTO `item_modified_appearance` (`ID`,`ItemID`,`ItemAppearanceModifierID`,`ItemAppearanceID`,`OrderIndex`,`TransmogSourceTypeEnum`,`Flags`,`VerifiedBuild`) VALUES
(311402,280062,0,63658,0,5,0,70938);
DELETE FROM `item_search_name` WHERE `ID` IN (2915);
INSERT INTO `item_search_name` (`ID`,`Display`,`OverallQualityID`,`ExpansionID`,`MinFactionID`,`MinReputation`,`AllowableClass`,`RequiredLevel`,`RequiredSkill`,`RequiredSkillRank`,`RequiredAbility`,`ItemLevel`,`Flags1`,`Flags2`,`Flags3`,`Flags4`,`Flags5`,`AllowableRace1`,`AllowableRace2`,`VerifiedBuild`) VALUES
(2915,'Taran Icebreaker',4,0,0,0,-1,47,0,0,0,52,0,16388,0,0,0,-1,-1,70938);
DELETE FROM `item_sparse` WHERE `ID` IN (2915);
INSERT INTO `item_sparse` (`ID`,`Description`,`Display3`,`Display2`,`Display1`,`Display`,`ExpansionID`,`DmgVariance`,`LimitCategory`,`DurationInInventory`,`QualityModifier`,`BagFamily`,`StartQuestID`,`LanguageID`,`ItemRange`,`StatPercentageOfSocket1`,`StatPercentageOfSocket2`,`StatPercentageOfSocket3`,`StatPercentageOfSocket4`,`StatPercentageOfSocket5`,`StatPercentageOfSocket6`,`StatPercentageOfSocket7`,`StatPercentageOfSocket8`,`StatPercentageOfSocket9`,`StatPercentageOfSocket10`,`StatPercentEditor1`,`StatPercentEditor2`,`StatPercentEditor3`,`StatPercentEditor4`,`StatPercentEditor5`,`StatPercentEditor6`,`StatPercentEditor7`,`StatPercentEditor8`,`StatPercentEditor9`,`StatPercentEditor10`,`StatModifierBonusStat1`,`StatModifierBonusStat2`,`StatModifierBonusStat3`,`StatModifierBonusStat4`,`StatModifierBonusStat5`,`StatModifierBonusStat6`,`StatModifierBonusStat7`,`StatModifierBonusStat8`,`StatModifierBonusStat9`,`StatModifierBonusStat10`,`Stackable`,`MaxCount`,`MinReputation`,`RequiredAbility`,`AllowableRace1`,`AllowableRace2`,`SellPrice`,`BuyPrice`,`VendorStackCount`,`PriceVariance`,`PriceRandomValue`,`Flags1`,`Flags2`,`Flags3`,`Flags4`,`Flags5`,`FactionRelated`,`ModifiedCraftingReagentItemID`,`ContentTuningID`,`PlayerLevelToItemLevelCurveID`,`ItemLevelOffsetCurveID`,`ItemLevelOffsetItemLevel`,`ItemSquishEraID`,`ItemNameDescriptionID`,`RequiredTransmogHoliday`,`RequiredHoliday`,`GemProperties`,`SocketMatchEnchantmentId`,`TotemCategoryID`,`InstanceBound`,`ZoneBound1`,`ZoneBound2`,`ItemSet`,`LockID`,`PageID`,`ItemDelay`,`MinFactionID`,`RequiredSkillRank`,`RequiredSkill`,`ItemLevel`,`AllowableClass`,`ArtifactID`,`SpellWeight`,`SpellWeightCategory`,`SocketType1`,`SocketType2`,`SocketType3`,`SheatheType`,`Material`,`PageMaterialID`,`Bonding`,`DamageDamageType`,`ContainerSlots`,`RequiredPVPMedal`,`RequiredPVPRank`,`RequiredLevel`,`InventoryType`,`OverallQualityID`,`AmmunitionType`,`VerifiedBuild`) VALUES
(2915,'','','','','Taran Icebreaker',0,0.4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,263,3684,0,0,0,0,0,0,0,0,91,7,-1,-1,-1,-1,-1,-1,-1,-1,1,0,0,0,-1,-1,55289,276447,1,1,1,0,16388,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3800,0,0,0,52,-1,0,0,0,0,0,0,1,2,0,2,0,0,0,0,47,17,4,0,70938);
DELETE FROM `quest_v2` WHERE `ID` IN (2768,2770,2846,2861,2864,2865,2936,2991,3042,3527);
INSERT INTO `quest_v2` (`ID`,`UniqueBitFlag`,`UiQuestDetailsTheme`,`VerifiedBuild`) VALUES
(2768,1646,0,70938),
(2770,1648,0,70938),
(2846,1664,0,70938),
(2861,1679,0,70938),
(2864,2071,0,70938),
(2865,2070,0,70938),
(2936,1716,0,70938),
(2991,1761,0,70938),
(3042,1775,0,70938),
(3527,1917,0,70938);
DELETE FROM `spell_categories` WHERE `ID` IN (191355);
INSERT INTO `spell_categories` (`ID`,`DifficultyID`,`Category`,`DefenseType`,`DiminishType`,`DispelType`,`Mechanic`,`PreventionType`,`StartRecoveryCategory`,`ChargeCategory`,`SpellID`,`VerifiedBuild`) VALUES
(191355,0,2615,0,0,0,0,0,0,0,1225469,70938);
DELETE FROM `spell_category` WHERE `ID` IN (2615);
INSERT INTO `spell_category` (`ID`,`Name`,`Flags`,`UsesPerWeek`,`MaxCharges`,`ChargeRecoveryTime`,`TypeMask`,`VerifiedBuild`) VALUES
(2615,'Dedicated Study',0,0,0,0,0,70938);
DELETE FROM `spell_cooldowns` WHERE `ID` IN (97019);
INSERT INTO `spell_cooldowns` (`ID`,`DifficultyID`,`CategoryRecoveryTime`,`RecoveryTime`,`StartRecoveryTime`,`AuraSpellID`,`SpellID`,`VerifiedBuild`) VALUES
(97019,0,82800000,0,0,0,1225469,70938);
DELETE FROM `spell_effect` WHERE `ID` IN (1129612,1129613);
INSERT INTO `spell_effect` (`ID`,`EffectAura`,`DifficultyID`,`EffectIndex`,`Effect`,`EffectAmplitude`,`EffectAttributes`,`EffectAuraPeriod`,`EffectBonusCoefficient`,`EffectChainAmplitude`,`EffectChainTargets`,`EffectItemType`,`EffectMechanic`,`EffectPointsPerResource`,`EffectPosFacing`,`EffectRealPointsPerLevel`,`EffectTriggerSpell`,`BonusCoefficientFromAP`,`PvpMultiplier`,`Coefficient`,`Variance`,`ResourceCoefficient`,`GroupSizeBasePointsCoefficient`,`EffectBasePoints`,`ScalingClass`,`TargetNodeGraph`,`EffectMiscValue1`,`EffectMiscValue2`,`EffectRadiusIndex1`,`EffectRadiusIndex2`,`EffectSpellClassMask1`,`EffectSpellClassMask2`,`EffectSpellClassMask3`,`EffectSpellClassMask4`,`ImplicitTarget1`,`ImplicitTarget2`,`SpellID`,`VerifiedBuild`) VALUES
(1129612,0,0,0,3,0,0,0,0,1,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,25,0,437589,70938),
(1129613,0,0,1,3,0,0,0,0,1,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,25,0,437589,70938);
DELETE FROM `spell_name` WHERE `ID` IN (1225469);
INSERT INTO `spell_name` (`ID`,`Name`,`VerifiedBuild`) VALUES
(1225469,'Dedicated Study',70938);
DELETE FROM `spell_visual` WHERE `ID` IN (197472);
INSERT INTO `spell_visual` (`ID`,`MissileCastOffset1`,`MissileCastOffset2`,`MissileCastOffset3`,`MissileImpactOffset1`,`MissileImpactOffset2`,`MissileImpactOffset3`,`StateKit`,`AnimEventSoundID`,`Flags`,`MissileAttachment`,`MissileDestinationAttachment`,`MissileCastPositionerID`,`MissileImpactPositionerID`,`MissileTargetingKit`,`HostileSpellVisualID`,`CasterSpellVisualID`,`SpellVisualMissileSetID`,`DamageNumberDelay`,`LowViolenceSpellVisualID`,`RaidSpellVisualMissileSetID`,`ReducedUnexpectedCameraMovementSpellVisualID`,`VerifiedBuild`) VALUES
(197472,0,0,0,0,0,0,286269,0,536870912,-1,34,0,0,0,0,0,0,0,0,0,0,70938);
DELETE FROM `spell_visual_kit` WHERE `ID` IN (289845);
INSERT INTO `spell_visual_kit` (`ID`,`ClutterLevel`,`FallbackSpellVisualKitId`,`DelayMin`,`DelayMax`,`MinimumSpellVisualDensityFilterType`,`MinimumSpellVisualDensityFilterParam`,`ReducedSpellVisualDensityFilterType`,`ReducedSpellVisualDensityFilterParam`,`Flags1`,`Flags2`,`VerifiedBuild`) VALUES
(289845,0,0,0,0,0,0,0,0,0,0,70938);
DELETE FROM `hotfix_blob` WHERE `locale`='enUS' AND (`TableHash`,`RecordId`) IN ((3776013982,1225469));
INSERT INTO `hotfix_blob` (`TableHash`,`RecordId`,`locale`,`Blob`,`VerifiedBuild`) VALUES
(3776013982,1225469,'enUS',0x00496e6372656173657320796f757220736b696c6c206279203120696e20796f7572206c6f77657374207472616465736b696c6c20616d6f6e6720796f7572205072696d61727920616e64205365636f6e64617279207472616465736b696c6c732e20496620796f75206861766520616c726561647920726561636865642033303020736b696c6c20696e20796f75722074776f2063757272656e74205072696d617279205472616465736b696c6c7320616e6420616c6c207468726565205365636f6e64617279205472616465736b696c6c732c20796f752077696c6c206761696e20247331206f6620612072616e646f6d20456c656d656e74616c20457373656e63652e0000,70938);
DELETE FROM `hotfix_data` WHERE `Id` BETWEEN 2086000 AND 2086999;
DELETE FROM `hotfix_data` WHERE (`TableHash`,`RecordId`) IN ((3014618826,39379),(3218799089,149196),(1829768651,135692),(1829768651,135693),(1829768651,135695),(1829768651,135696),(1829768651,135697),(1829768651,135698),(1829768651,135699),(331613093,707086),(1344507586,2915),(3834752085,311402),(3645674820,311402),(428746933,2915),(2442913102,2915),(3465080266,2915),(986198281,2768),(986198281,2770),(986198281,2846),(986198281,2861),(986198281,2864),(986198281,2865),(986198281,2936),(986198281,2991),(986198281,3042),(986198281,3527),(3776013982,1225469),(3689412649,191355),(3502494094,2615),(4193483863,97019),(4030871717,1129612),(4030871717,1129613),(1187407512,1225469),(4146370265,197472),(4102286043,289845),(1417785224,2915));
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(2086000,1397806000,3014618826,39379,1,70938),
(2086001,1397806001,3218799089,149196,1,70938),
(2086002,1397806002,1829768651,135692,1,70938),
(2086003,1397806003,1829768651,135693,1,70938),
(2086004,1397806004,1829768651,135695,1,70938),
(2086005,1397806005,1829768651,135696,1,70938),
(2086006,1397806006,1829768651,135697,1,70938),
(2086007,1397806007,1829768651,135698,1,70938),
(2086008,1397806008,1829768651,135699,1,70938),
(2086009,1397806009,331613093,707086,3,70938),
(2086010,1397806010,1344507586,2915,1,70938),
(2086011,1397806011,3834752085,311402,1,70938),
(2086012,1397806012,3645674820,311402,2,70938),
(2086013,1397806013,428746933,2915,1,70938),
(2086014,1397806014,2442913102,2915,1,70938),
(2086015,1397806015,3465080266,2915,3,70938),
(2086016,1397806016,986198281,2768,1,70938),
(2086017,1397806017,986198281,2770,1,70938),
(2086018,1397806018,986198281,2846,1,70938),
(2086019,1397806019,986198281,2861,1,70938),
(2086020,1397806020,986198281,2864,1,70938),
(2086021,1397806021,986198281,2865,1,70938),
(2086022,1397806022,986198281,2936,1,70938),
(2086023,1397806023,986198281,2991,1,70938),
(2086024,1397806024,986198281,3042,1,70938),
(2086025,1397806025,986198281,3527,1,70938),
(2086026,1397806026,3776013982,1225469,1,70938),
(2086027,1397806027,3689412649,191355,1,70938),
(2086028,1397806028,3502494094,2615,1,70938),
(2086029,1397806029,4193483863,97019,1,70938),
(2086030,1397806030,4030871717,1129612,1,70938),
(2086031,1397806031,4030871717,1129613,1,70938),
(2086032,1397806032,1187407512,1225469,1,70938),
(2086033,1397806033,4146370265,197472,1,70938),
(2086034,1397806034,4102286043,289845,1,70938),
(2086035,1397806035,1417785224,2915,3,70938);
