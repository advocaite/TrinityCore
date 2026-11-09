-- Classic 1.60 (WoW Forever): Ironforge had no shaman trainer (Discord, Russian player 2026-10-31: "no shaman trainer in Ironforge,
-- .npc add does not work either").
-- Forever's Ironforge shaman trainer is Eldrun Stormbreaker (258098, <Shaman Trainer>, Ironforge faction, Mystic Ward). Our world had
-- his gossip and trainer rows (2026_10_27_71) but no creature_template and no spawn: both only came with the ported world files that
-- the 0.1.27.x builds leave out, so the NPC did not exist (.npc add 258098 answers that the entry does not exist) and the dwarf
-- shamans had only Teo Hammerstorm (Coldridge) and Ingrid Dunwald (Kharanos).
--
-- Data source: Azeroth project, Sakhwoow/WoWForever (GPL-2), branch forever, official-server sniffs (build 70425):
--   sql/custom/world/2026_10_20_00_world_forever_actual_sniff_01_creature_template.sql (template, difficulty, model, equipment,
--   level 5, ruRU name), ..._09_spawns.sql (spawn -4642.36 -1086.46 500.64, Ironforge), and their fix
--   2026_10_25_04_world_ironforge_shaman_trainer_riley_trainer.sql (trainer flag, trainer list 10 and menu 41915 as Ingrid Dunwald
--   258113, a map pin for the Ironforge guards' "Shaman" option). English name from Wowhead Forever.
-- Ours on top: class trainer flag 32 and trainer_class 7 (shaman), so the talent reset works as at the other class trainers
-- (same as 2026_10_31_50); every other spawn of 258098 is removed so he never stands twice (an older ported spawn on the same spot,
-- or the map-point spawns on the mountain above the city, z 720+, that a quest-giver spawn list could add).
-- Safe to run again.

-- template (sniff 70425)
DELETE FROM `creature_template` WHERE `entry` = 258098;
INSERT INTO `creature_template` (`entry`,`KillCredit1`,`KillCredit2`,`name`,`femaleName`,`subname`,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,`npcflag`,`speed_walk`,`speed_run`,`scale`,`Classification`,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,`family`,`trainer_class`,`type`,`VehicleId`,`AIName`,`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,`ScriptName`,`StringId`,`VerifiedBuild`) VALUES
(258098,0,0,'Eldrun Stormbreaker',NULL,'Shaman Trainer',NULL,NULL,0,0,55,51,1.0,1.142857,1.0,0,0,2000,2000,1.0,1.0,2,768,2048,0,0,7,7,0,'',0,1.0,0,1693,0,0,1,0,0,'',NULL,70425);
-- npcflag 51 = 1 gossip | 2 quest giver (sniff) | 16 trainer | 32 class trainer

DELETE FROM `creature_template_difficulty` WHERE `Entry` = 258098;
INSERT INTO `creature_template_difficulty` (`Entry`,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,`LootID`,`PickPocketLootID`,`SkinLootID`,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild`) VALUES
(258098,0,0,0,0,0,1.0,1.0,1.0,1.0,341192,134217728,67108864,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70425);

DELETE FROM `creature_template_model` WHERE `CreatureID` = 258098;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES
(258098,0,143525,1.0,1.0,70425);

DELETE FROM `creature_equip_template` WHERE `CreatureID` = 258098;
INSERT INTO `creature_equip_template` (`CreatureID`,`ID`,`ItemID1`,`AppearanceModID1`,`ItemVisual1`,`ItemID2`,`AppearanceModID2`,`ItemVisual2`,`ItemID3`,`AppearanceModID3`,`ItemVisual3`,`VerifiedBuild`) VALUES
(258098,1,6334,0,0,0,0,0,0,0,0,70425);

DELETE FROM `creature_classic_level` WHERE `entry` = 258098;
INSERT INTO `creature_classic_level` (`entry`,`level_min`,`level_max`) VALUES (258098,5,5);

DELETE FROM `creature_template_locale` WHERE `entry` = 258098 AND `locale` = 'ruRU';
INSERT INTO `creature_template_locale` (`entry`,`locale`,`Name`,`Title`) VALUES
(258098,'ruRU','Элдрун Тучегон','Наставник шаманов');

-- trainer: the full shaman list 10 and the training menu 41915, as Ingrid Dunwald (258113, same rows as 2026_10_27_71)
DELETE FROM `creature_template_gossip` WHERE `CreatureID` = 258098;
INSERT INTO `creature_template_gossip` (`CreatureID`,`MenuID`,`VerifiedBuild`) VALUES (258098,41915,0);
DELETE FROM `creature_trainer` WHERE `CreatureID` = 258098;
INSERT INTO `creature_trainer` (`CreatureID`,`TrainerID`,`MenuID`,`OptionID`) VALUES (258098,10,41915,0);

-- spawn (sniff 70425), Ironforge, Mystic Ward; guid 46001000 (ours)
DELETE FROM `creature_addon` WHERE `guid` IN (SELECT `guid` FROM `creature` WHERE `id` = 258098);
DELETE FROM `creature` WHERE `id` = 258098 OR `guid` = 46001000;
INSERT INTO `creature` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`modelid`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`currentwaypoint`,`curHealthPct`,`MovementType`,`npcflag`,`unit_flags`,`unit_flags2`,`unit_flags3`,`ScriptName`,`StringId`,`VerifiedBuild`) VALUES
(46001000,258098,0,1537,1537,'0',0,0,0,-1,0,1,-4642.36279,-1086.46008,500.63712,0.0,120,0.0,0,100,0,0,0,0,0,'',NULL,70425);

-- Ironforge guards, class trainer menu 2144, option 7 "Shaman": the sniff has the option but not its map pin. Pin on Eldrun.
-- Only where the option has no pin yet.
DELETE FROM `points_of_interest` WHERE `ID` = 900101;
INSERT INTO `points_of_interest` (`ID`,`PositionX`,`PositionY`,`PositionZ`,`Icon`,`Flags`,`Importance`,`Name`,`WMOGroupID`,`VerifiedBuild`) VALUES
(900101,-4642.4,-1086.5,500.6,7,99,0,'Ironforge Shaman Trainer',0,0);
UPDATE `gossip_menu_option` SET `ActionPoiID` = 900101 WHERE `MenuID` = 2144 AND `OptionID` = 7 AND `ActionPoiID` = 0;
