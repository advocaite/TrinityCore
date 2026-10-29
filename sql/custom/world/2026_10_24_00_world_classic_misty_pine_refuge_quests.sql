-- Father Gavin (1253, The Argent Dawn) at Misty Pine Refuge, Dun Morogh, per Wowhead Forever:
-- 1. He ends "Dawn in the Mountains" (99158) but had npcflag 0, so no quest marker and no turn-in. Gossip + questgiver.
-- 2. He starts and ends 99159 Finding Warmth, 99160/99161 Rime's Wrath and 99162 Treacherous Cold, which had no
--    creature_queststarter/questender rows. Wowhead gives no chain order; Avala's Rime's Wrath (99161) follows the
--    elementals one (99160).
-- 3. What those quests need was missing. Spawn points are Wowhead Forever map coordinates (uiMap 1426) converted with a
--    linear fit on 8 NPCs spawned in both (max error ~5.5 yd), Z read from the extracted .map terrain: approximate, not
--    sniffed positions.
--    - 99160: Minor Ice Elemental (276003) had no spawns: 21 spawns (Wowhead lists 22, one point is Father Gavin's).
--      Faction 35 (friendly) -> 91 (Elemental, hostile) like the other elementals.
--    - 99161: Avala (276009, "The Lost Rime", display 146644) did not exist: template copied from 276003, the 4 Wowhead
--      spawn points in a pool of 1, drops Avala's Core (286325) while on the quest. Both get their Wowhead loot.
--    - 99162: Coalbeard's (695222), Sunhammer's (695223) and Stoneanvil's (695277) Rifle objects did not exist: quest
--      chests (display 88133, lock 259 like other quest pickups), one spawn each, 0.5 yd above the terrain (the model
--      is centered on its origin, so it was half buried and hard to see).
-- 4. The quests had no quest_poi, so the map showed nothing. Same layout as the sniffed 1.60 quests (98319, 98321):
--    the quest giver point (ObjectiveIndex -1), one blob per objective (convex hull of its spawns padded 12 yd, a 48 yd
--    square around single spawns), then the turn-in point (ObjectiveIndex 32), all at Father Gavin.
START TRANSACTION;

-- 1. Father Gavin
UPDATE `creature_template` SET `npcflag` = `npcflag` | 0x3 WHERE `entry` = 1253;

-- 2. Quests
DELETE FROM `creature_queststarter` WHERE `id` = 1253 AND `quest` IN (99159, 99160, 99161, 99162);
INSERT INTO `creature_queststarter` (`id`, `quest`, `VerifiedBuild`) VALUES
(1253, 99159, 0), (1253, 99160, 0), (1253, 99161, 0), (1253, 99162, 0);

DELETE FROM `creature_questender` WHERE `id` = 1253 AND `quest` IN (99159, 99160, 99161, 99162);
INSERT INTO `creature_questender` (`id`, `quest`, `VerifiedBuild`) VALUES
(1253, 99159, 0), (1253, 99160, 0), (1253, 99161, 0), (1253, 99162, 0);

-- Rime's Wrath: Avala (99161) is offered once the elementals one (99160) is done
INSERT INTO `quest_template_addon` (`ID`, `PrevQuestID`) VALUES (99161, 99160)
ON DUPLICATE KEY UPDATE `PrevQuestID` = 99160;

-- 3. Quest creatures and objects
-- Minor Ice Elemental
UPDATE `creature_template` SET `faction` = 91 WHERE `entry` = 276003;

-- Avala
DELETE FROM `creature_template` WHERE `entry` = 276009;
INSERT INTO `creature_template` (`entry`, `KillCredit1`, `KillCredit2`, `name`, `femaleName`, `subname`, `TitleAlt`, `IconName`, `RequiredExpansion`, `VignetteID`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `scale`, `Classification`, `dmgschool`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`, `unit_class`, `unit_flags`, `unit_flags2`, `unit_flags3`, `family`, `trainer_class`, `type`, `VehicleId`, `AIName`, `MovementType`, `ExperienceModifier`, `RacialLeader`, `movementId`, `WidgetSetID`, `WidgetSetUnitConditionID`, `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`, `StringId`, `VerifiedBuild`)
SELECT 276009, 0, 0, 'Avala', `femaleName`, 'The Lost Rime', `TitleAlt`, `IconName`, `RequiredExpansion`, `VignetteID`, 91, 0, `speed_walk`, `speed_run`, `scale`, `Classification`, `dmgschool`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`, `unit_class`, `unit_flags`, `unit_flags2`, `unit_flags3`, `family`, `trainer_class`, `type`, `VehicleId`, `AIName`, `MovementType`, `ExperienceModifier`, `RacialLeader`, `movementId`, `WidgetSetID`, `WidgetSetUnitConditionID`, `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, '', NULL, 0
  FROM `creature_template` WHERE `entry` = 276003;

DELETE FROM `creature_template_difficulty` WHERE `Entry` = 276009;
CREATE TEMPORARY TABLE `tmp_avala` AS SELECT * FROM `creature_template_difficulty` WHERE `Entry` = 276003;
UPDATE `tmp_avala` SET `Entry` = 276009, `HealthModifier` = 2, `LootID` = 276009, `VerifiedBuild` = 0;
INSERT INTO `creature_template_difficulty` SELECT * FROM `tmp_avala`;
DROP TEMPORARY TABLE `tmp_avala`;

-- No creature_classic_level row, so both were level 1: Minor Ice Elemental 7-8, Avala 8 (Wowhead Forever).
DELETE FROM `creature_classic_level` WHERE `entry` IN (276003, 276009);
INSERT INTO `creature_classic_level` (`entry`, `level_min`, `level_max`) VALUES
(276003, 7, 8),
(276009, 8, 8);

-- Display 146644 has no creature_model_info, without which Creature::Create refuses to load Avala. It is drawn at
-- scale 1 against the Minor Ice Elemental's 12310 at 0.5: twice 12310's bounding radius, same combat reach.
DELETE FROM `creature_model_info` WHERE `DisplayID` = 146644;
INSERT INTO `creature_model_info` (`DisplayID`, `BoundingRadius`, `CombatReach`, `DisplayID_Other_Gender`, `VerifiedBuild`) VALUES
(146644, 1.74, 1.5, 0, 0);

DELETE FROM `creature_template_model` WHERE `CreatureID` = 276009;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`) VALUES
(276009, 0, 146644, 1, 1, 0);

-- Loot (Wowhead Forever drop rates): Minor Ice Elemental had no loot id. Grey gear is the low-level reference 30017 (one item
-- per roll) at the summed Wowhead rate; rare recipes/pouch through 30019 like the other Dun Morogh mobs.
UPDATE `creature_template_difficulty` SET `LootID` = 276003 WHERE `Entry` = 276003 AND `DifficultyID` = 0;
DELETE FROM `creature_loot_template` WHERE `Entry` IN (276003, 276009);
INSERT INTO `creature_loot_template` (`Entry`, `ItemType`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(276003, 0, 5446, 44.4, 0, 1, 0, 1, 1, 'Minor Ice Elemental - Broken Elemental Bracer'),
(276003, 0, 4877, 24.4, 0, 1, 0, 1, 1, 'Minor Ice Elemental - Stone Arrowhead'),
(276003, 1, 30017, 43, 0, 1, 0, 1, 1, 'Minor Ice Elemental - low level grey gear'),
(276003, 1, 30019, 0.5, 0, 1, 0, 1, 1, 'Minor Ice Elemental - low level rares'),
(276009, 0, 286325, 100, 1, 1, 0, 1, 1, 'Avala - Avala''s Core'),
(276009, 0, 5446, 12.1, 0, 1, 0, 1, 1, 'Avala - Broken Elemental Bracer'),
(276009, 0, 4877, 4.8, 0, 1, 0, 1, 1, 'Avala - Stone Arrowhead'),
(276009, 1, 30017, 4.5, 0, 1, 0, 1, 1, 'Avala - low level grey gear');

-- Rifles
DELETE FROM `gameobject_template` WHERE `entry` IN (695222, 695223, 695277);
INSERT INTO `gameobject_template` (`entry`, `type`, `displayId`, `name`, `IconName`, `castBarCaption`, `unk1`, `size`, `Data0`, `Data1`, `Data3`, `Data8`, `ContentTuningId`, `RequiredLevel`, `AIName`, `ScriptName`, `VerifiedBuild`) VALUES
(695222, 3, 88133, 'Coalbeard''s Rifle',  '', '', '', 1, 259, 695222, 1, 99162, 0, 0, '', '', 0),
(695223, 3, 88133, 'Sunhammer''s Rifle',  '', '', '', 1, 259, 695223, 1, 99162, 0, 0, '', '', 0),
(695277, 3, 88133, 'Stoneanvil''s Rifle', '', '', '', 1, 259, 695277, 1, 99162, 0, 0, '', '', 0);

DELETE FROM `gameobject_loot_template` WHERE `Entry` IN (695222, 695223, 695277);
INSERT INTO `gameobject_loot_template` (`Entry`, `ItemType`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(695222, 0, 286358, 100, 1, 1, 0, 1, 1, 'Coalbeard''s Rifle'),
(695223, 0, 286359, 100, 1, 1, 0, 1, 1, 'Sunhammer''s Rifle'),
(695277, 0, 286360, 100, 1, 1, 0, 1, 1, 'Stoneanvil''s Rifle');

-- Spawns (guid ranges 26300000-26300099 and 34100000-34100009 reserved for this file)
DELETE FROM `pool_members` WHERE `type` = 0 AND `spawnId` BETWEEN 26300000 AND 26300099;
DELETE FROM `pool_template` WHERE `entry` = 1310000;
DELETE FROM `creature` WHERE `guid` BETWEEN 26300000 AND 26300099;
DELETE FROM `gameobject` WHERE `guid` BETWEEN 34100000 AND 34100009;
INSERT INTO `creature` (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnDifficulties`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`, `MovementType`, `VerifiedBuild`) VALUES
(26300000, 276003, 0, 0, 0, '0', -5324.87, -763.77, 388.58, 0.84, 300, 5, 1, 0),
(26300001, 276003, 0, 0, 0, '0', -5324.87, -793.4, 388.33, 5.32, 300, 5, 1, 0),
(26300002, 276003, 0, 0, 0, '0', -5341.23, -808.22, 393.95, 4.80, 300, 5, 1, 0),
(26300003, 276003, 0, 0, 0, '0', -5292.14, -813.16, 395.99, 1.60, 300, 5, 1, 0),
(26300004, 276003, 0, 0, 0, '0', -5292.14, -842.8, 404.26, 3.11, 300, 5, 1, 0),
(26300005, 276003, 0, 0, 0, '0', -5331.41, -842.8, 388.33, 2.82, 300, 5, 1, 0),
(26300006, 276003, 0, 0, 0, '0', -5377.23, -862.56, 391.35, 4.09, 300, 5, 1, 0),
(26300007, 276003, 0, 0, 0, '0', -5367.41, -872.44, 391.23, 4.95, 300, 5, 1, 0),
(26300008, 276003, 0, 0, 0, '0', -5377.23, -911.95, 394.07, 0.59, 300, 5, 1, 0),
(26300009, 276003, 0, 0, 0, '0', -5337.96, -931.71, 389.18, 0.18, 300, 5, 1, 0),
(26300010, 276003, 0, 0, 0, '0', -5341.23, -931.71, 388.37, 5.25, 300, 5, 1, 0),
(26300011, 276003, 0, 0, 0, '0', -5305.23, -961.35, 389.89, 2.72, 300, 5, 1, 0),
(26300012, 276003, 0, 0, 0, '0', -5351.05, -966.29, 388.33, 4.79, 300, 5, 1, 0),
(26300013, 276003, 0, 0, 0, '0', -5390.32, -971.23, 389.49, 0.01, 300, 5, 1, 0),
(26300014, 276003, 0, 0, 0, '0', -5321.6, -976.16, 388.33, 2.80, 300, 5, 1, 0),
(26300015, 276003, 0, 0, 0, '0', -5383.77, -1005.8, 388.33, 4.53, 300, 5, 1, 0),
(26300016, 276003, 0, 0, 0, '0', -5337.96, -1010.74, 388.33, 1.44, 300, 5, 1, 0),
(26300017, 276003, 0, 0, 0, '0', -5354.32, -1015.68, 388.65, 5.94, 300, 5, 1, 0),
(26300018, 276003, 0, 0, 0, '0', -5305.23, -1030.5, 392.6, 5.66, 300, 5, 1, 0),
(26300019, 276003, 0, 0, 0, '0', -5334.69, -1035.44, 396.42, 0.19, 300, 5, 1, 0),
(26300020, 276003, 0, 0, 0, '0', -5298.69, -1040.38, 400.93, 0.16, 300, 5, 1, 0);
INSERT INTO `creature` (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnDifficulties`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`, `MovementType`, `VerifiedBuild`) VALUES
(26300050, 276009, 0, 0, 0, '0', -5311.78, -1020.62, 388.33, 3.40, 600, 3, 1, 0),
(26300051, 276009, 0, 0, 0, '0', -5265.97, -1030.5, 395.24, 5.90, 600, 3, 1, 0),
(26300052, 276009, 0, 0, 0, '0', -5279.06, -1060.14, 409.26, 2.39, 600, 3, 1, 0),
(26300053, 276009, 0, 0, 0, '0', -5259.42, -1070.02, 405.8, 1.36, 600, 3, 1, 0);
INSERT INTO `gameobject` (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnDifficulties`, `position_x`, `position_y`, `position_z`, `orientation`, `rotation0`, `rotation1`, `rotation2`, `rotation3`, `spawntimesecs`, `animprogress`, `state`, `VerifiedBuild`) VALUES
(34100000, 695222, 0, 0, 0, '0', -5324.87, -768.71, 388.83, 0, 0, 0, 0, 1, 60, 255, 1, 0),
(34100001, 695223, 0, 0, 0, '0', -5524.48, -1153.99, 402.64, 0, 0, 0, 0, 1, 60, 255, 1, 0),
(34100002, 695277, 0, 0, 0, '0', -5805.91, -823.04, 397.99, 0, 0, 0, 0, 1, 60, 255, 1, 0);

INSERT INTO `pool_template` (`entry`, `max_limit`, `description`) VALUES (1310000, 1, 'Avala (276009) - Dun Morogh');
INSERT INTO `pool_members` (`type`, `spawnId`, `poolSpawnId`, `chance`, `description`) VALUES
(0, 26300050, 1310000, 0, 'Avala'), (0, 26300051, 1310000, 0, 'Avala'),
(0, 26300052, 1310000, 0, 'Avala'), (0, 26300053, 1310000, 0, 'Avala');

-- 4. Quest POIs
DELETE FROM `quest_poi` WHERE `QuestID` IN (99159, 99160, 99161, 99162);
DELETE FROM `quest_poi_points` WHERE `QuestID` IN (99159, 99160, 99161, 99162);
INSERT INTO `quest_poi` (`QuestID`, `BlobIndex`, `Idx1`, `ObjectiveIndex`, `QuestObjectiveID`, `QuestObjectID`, `MapID`, `UiMapID`, `Priority`, `Flags`, `WorldEffectID`, `PlayerConditionID`, `NavigationPlayerConditionID`, `SpawnTrackingID`, `AlwaysAllowMergingBlobs`, `VerifiedBuild`) VALUES
(99159, 0, 0, -1, 0, 0, 0, 1426, 0, 0, 0, 0, 0, 0, 0, 0),
(99159, 0, 1, 0, 479520, 286339, 0, 1426, 0, 0, 0, 0, 0, 0, 0, 0),
(99159, 0, 2, 32, 0, 0, 0, 1426, 0, 0, 0, 0, 0, 0, 0, 0),
(99160, 0, 0, -1, 0, 0, 0, 1426, 0, 0, 0, 0, 0, 0, 0, 0),
(99160, 0, 1, 0, 479482, 276003, 0, 1426, 0, 0, 0, 0, 0, 0, 0, 0),
(99160, 0, 2, 32, 0, 0, 0, 1426, 0, 0, 0, 0, 0, 0, 0, 0),
(99161, 0, 0, -1, 0, 0, 0, 1426, 0, 0, 0, 0, 0, 0, 0, 0),
(99161, 0, 1, 0, 479481, 286325, 0, 1426, 0, 0, 0, 0, 0, 0, 0, 0),
(99161, 0, 2, 32, 0, 0, 0, 1426, 0, 0, 0, 0, 0, 0, 0, 0),
(99162, 0, 0, -1, 0, 0, 0, 1426, 0, 0, 0, 0, 0, 0, 0, 0),
(99162, 0, 1, 0, 479527, 286360, 0, 1426, 0, 0, 0, 0, 0, 0, 0, 0),
(99162, 0, 2, 1, 479528, 286359, 0, 1426, 0, 0, 0, 0, 0, 0, 0, 0),
(99162, 0, 3, 2, 479529, 286358, 0, 1426, 0, 0, 0, 0, 0, 0, 0, 0),
(99162, 0, 4, 32, 0, 0, 0, 1426, 0, 0, 0, 0, 0, 0, 0, 0);
INSERT INTO `quest_poi_points` (`QuestID`, `Idx1`, `Idx2`, `X`, `Y`, `Z`, `VerifiedBuild`) VALUES
(99159, 0, 0, -5351, -1041, 395, 0),
(99159, 1, 0, -5598, -733, 0, 0),
(99159, 1, 1, -5556, -751, 0, 0),
(99159, 1, 2, -5504, -694, 0, 0),
(99159, 1, 3, -5577, -636, 0, 0),
(99159, 2, 0, -5351, -1041, 395, 0),
(99160, 0, 0, -5351, -1041, 395, 0),
(99160, 1, 0, -5399, -980, 0, 0),
(99160, 1, 1, -5389, -1016, 0, 0),
(99160, 1, 2, -5334, -1047, 0, 0),
(99160, 1, 3, -5295, -1052, 0, 0),
(99160, 1, 4, -5286, -833, 0, 0),
(99160, 1, 5, -5287, -802, 0, 0),
(99160, 1, 6, -5324, -752, 0, 0),
(99160, 1, 7, -5384, -853, 0, 0),
(99160, 2, 0, -5351, -1041, 395, 0),
(99161, 0, 0, -5351, -1041, 395, 0),
(99161, 1, 0, -5321, -1013, 0, 0),
(99161, 1, 1, -5279, -1072, 0, 0),
(99161, 1, 2, -5252, -1079, 0, 0),
(99161, 1, 3, -5258, -1022, 0, 0),
(99161, 2, 0, -5351, -1041, 395, 0),
(99162, 0, 0, -5351, -1041, 395, 0),
(99162, 1, 0, -5830, -847, 0, 0),
(99162, 1, 1, -5782, -847, 0, 0),
(99162, 1, 2, -5782, -799, 0, 0),
(99162, 1, 3, -5830, -799, 0, 0),
(99162, 2, 0, -5548, -1178, 0, 0),
(99162, 2, 1, -5500, -1178, 0, 0),
(99162, 2, 2, -5500, -1130, 0, 0),
(99162, 2, 3, -5548, -1130, 0, 0),
(99162, 3, 0, -5349, -793, 0, 0),
(99162, 3, 1, -5301, -793, 0, 0),
(99162, 3, 2, -5301, -745, 0, 0),
(99162, 3, 3, -5349, -745, 0, 0),
(99162, 4, 0, -5351, -1041, 395, 0);
COMMIT;
