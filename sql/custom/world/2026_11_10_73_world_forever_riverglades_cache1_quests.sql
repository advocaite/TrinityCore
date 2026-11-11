-- Riverglades (WoW Forever, Classic 1.60): six more of the zone's quests, from one player's live game cache (Onlybeams, Cache 1,
-- enUS build 70862, sent 2026-11-10) read as data only with tools/forever_cache. None of them needs a script.
--   95912 WANTED: Blademaster Kaijo, 95991 WANTED: Ghostmaw (Farholde Keep's WANTED! board), 95119 Unfriendly Competition and
--   95288 Smuggler's Paradise (Powderfuse Port), 94889 Carving a Path (Sergeant Riftan, Twilight's Shroud), 94679 Snargl's Snout
--   (Cedric Dalton, Southern Watch).
-- Text, levels, objectives, money, reputation and races are the cache's own record. Who gives each quest: the player's QuestieTrace
-- (2026-11-08) and Questie's Forever data (QuestieDB aca0da1, GPLv3); who takes it back: the quest's own log line.
-- The cache has no reward text, so that is a short placeholder (VerifiedBuild -1 marks it).
-- New NPCs (Ghostmaw, Eastsea Buccaneer and Musketeer, Twilight Corrupter, Snargl): name, models, type, family, class, elite and
-- health from the cache; faction, attack and the rest copied from a related NPC of ours (named below); levels from Questie or the
-- quest level. Which NPCs carry which quest item: the cache (creaturecache quest items).
-- ESTIMATED, check in game:
--   * Spots. Questie (Wowhead) map points turned into world spots with a fit from 111 of our own single-spawn NPCs in the zone
--     (median error 0.6 yd) where Questie has them: Ghostmaw, the WANTED! board (the player's position at the board).
--     Nobody has spots for the pirates, the Corrupters and Snargl: pirates are placed around our only Eastsea Raider on the
--     Eastwind Shore, Corrupters next to our Twilight Fanatics and Champions, Snargl in the middle of our gnoll camp.
--   * Heights: from our nearest spawn, NPCs 2 yd higher (the server puts them on the ground when they move). The object uses the
--     height of the NPC next to it. rg_heights.py (riverglades-0134) can put every spot on the ground from the server's .map files.
--   * Drop chances: the item that is the quest's only goal 100 %, Smuggled Goods 40 %.
-- Also: Sergeant Riftan (Alliance quest giver) was on faction 7 and could be attacked; now 12 like Farholde. Cedric Dalton was level 1.
-- Safe to run again: every key is deleted first. Guid blocks 47221000-47221099 (creature) and 47231000-47231009 (gameobject).

-- A. NPCs we lacked (5)
-- 263380 Ghostmaw: copy of 259046 Freshwater Crocolisk, level 42 (Questie)
DELETE FROM `creature_template` WHERE `entry`=263380;
DELETE FROM `creature_template_model` WHERE `CreatureID`=263380;
DELETE FROM `creature_template_difficulty` WHERE `Entry`=263380;
DELETE FROM `creature_classic_level` WHERE `entry`=263380;
INSERT INTO `creature_template` (`entry`,`KillCredit1`,`KillCredit2`,`name`,`femaleName`,`subname`,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,`npcflag`,`speed_walk`,`speed_run`,`scale`,`Classification`,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,`family`,`trainer_class`,`type`,`VehicleId`,`AIName`,`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,`ScriptName`,`StringId`,`VerifiedBuild`) SELECT 263380,0,0,'Ghostmaw',`femaleName`,NULL,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,0,`speed_walk`,`speed_run`,`scale`,1,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,1,`unit_flags`,`unit_flags2`,`unit_flags3`,6,`trainer_class`,1,`VehicleId`,'',`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,'',`StringId`,70862 FROM `creature_template` WHERE `entry`=259046;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES (263380,0,2850,1.0,1.0,70862);
INSERT INTO `creature_template_difficulty` (`Entry`,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,`LootID`,`PickPocketLootID`,`SkinLootID`,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild`) SELECT 263380,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,3.2,1.0,`ArmorModifier`,`DamageModifier`,346687,1,0,`TypeFlags3`,0,0,0,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,70862 FROM `creature_template_difficulty` WHERE `Entry`=259046 AND `DifficultyID`=0;
INSERT INTO `creature_classic_level` (`entry`,`level_min`,`level_max`) VALUES (263380,42,42);
-- 257901 Eastsea Buccaneer: copy of 257915 Eastsea Raider, level 42-43 (quest level 43)
DELETE FROM `creature_template` WHERE `entry`=257901;
DELETE FROM `creature_template_model` WHERE `CreatureID`=257901;
DELETE FROM `creature_template_difficulty` WHERE `Entry`=257901;
DELETE FROM `creature_classic_level` WHERE `entry`=257901;
INSERT INTO `creature_template` (`entry`,`KillCredit1`,`KillCredit2`,`name`,`femaleName`,`subname`,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,`npcflag`,`speed_walk`,`speed_run`,`scale`,`Classification`,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,`family`,`trainer_class`,`type`,`VehicleId`,`AIName`,`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,`ScriptName`,`StringId`,`VerifiedBuild`) SELECT 257901,0,0,'Eastsea Buccaneer',`femaleName`,NULL,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,0,`speed_walk`,`speed_run`,`scale`,0,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,1,`unit_flags`,`unit_flags2`,`unit_flags3`,0,`trainer_class`,7,`VehicleId`,'',`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,'',`StringId`,70862 FROM `creature_template` WHERE `entry`=257915;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES (257901,0,140452,1.0,1.0,70862),(257901,1,140453,1.0,1.0,70862);
INSERT INTO `creature_template_difficulty` (`Entry`,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,`LootID`,`PickPocketLootID`,`SkinLootID`,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild`) SELECT 257901,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,1.2,1.0,`ArmorModifier`,`DamageModifier`,340992,0,0,`TypeFlags3`,0,0,0,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,70862 FROM `creature_template_difficulty` WHERE `Entry`=257915 AND `DifficultyID`=0;
INSERT INTO `creature_classic_level` (`entry`,`level_min`,`level_max`) VALUES (257901,42,43);
-- 257910 Eastsea Musketeer: copy of 257915 Eastsea Raider, level 42-43 (quest level 43)
DELETE FROM `creature_template` WHERE `entry`=257910;
DELETE FROM `creature_template_model` WHERE `CreatureID`=257910;
DELETE FROM `creature_template_difficulty` WHERE `Entry`=257910;
DELETE FROM `creature_classic_level` WHERE `entry`=257910;
INSERT INTO `creature_template` (`entry`,`KillCredit1`,`KillCredit2`,`name`,`femaleName`,`subname`,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,`npcflag`,`speed_walk`,`speed_run`,`scale`,`Classification`,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,`family`,`trainer_class`,`type`,`VehicleId`,`AIName`,`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,`ScriptName`,`StringId`,`VerifiedBuild`) SELECT 257910,0,0,'Eastsea Musketeer',`femaleName`,NULL,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,0,`speed_walk`,`speed_run`,`scale`,0,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,1,`unit_flags`,`unit_flags2`,`unit_flags3`,0,`trainer_class`,7,`VehicleId`,'',`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,'',`StringId`,70862 FROM `creature_template` WHERE `entry`=257915;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES (257910,0,140456,1.0,1.0,70862),(257910,1,140457,1.0,1.0,70862);
INSERT INTO `creature_template_difficulty` (`Entry`,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,`LootID`,`PickPocketLootID`,`SkinLootID`,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild`) SELECT 257910,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,1.2,1.0,`ArmorModifier`,`DamageModifier`,341001,134217728,0,`TypeFlags3`,0,0,0,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,70862 FROM `creature_template_difficulty` WHERE `Entry`=257915 AND `DifficultyID`=0;
INSERT INTO `creature_classic_level` (`entry`,`level_min`,`level_max`) VALUES (257910,42,43);
-- 256797 Twilight Corrupter: copy of 256795 Twilight Fanatic, level 42-43 (Questie 42)
DELETE FROM `creature_template` WHERE `entry`=256797;
DELETE FROM `creature_template_model` WHERE `CreatureID`=256797;
DELETE FROM `creature_template_difficulty` WHERE `Entry`=256797;
DELETE FROM `creature_classic_level` WHERE `entry`=256797;
INSERT INTO `creature_template` (`entry`,`KillCredit1`,`KillCredit2`,`name`,`femaleName`,`subname`,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,`npcflag`,`speed_walk`,`speed_run`,`scale`,`Classification`,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,`family`,`trainer_class`,`type`,`VehicleId`,`AIName`,`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,`ScriptName`,`StringId`,`VerifiedBuild`) SELECT 256797,0,0,'Twilight Corrupter',`femaleName`,NULL,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,0,`speed_walk`,`speed_run`,`scale`,0,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,8,`unit_flags`,`unit_flags2`,`unit_flags3`,0,`trainer_class`,7,`VehicleId`,'',`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,'',`StringId`,70862 FROM `creature_template` WHERE `entry`=256795;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES (256797,0,139817,1.0,1.0,70862),(256797,1,139818,1.0,1.0,70862),(256797,2,140334,1.0,0.75,70862),(256797,3,140328,1.0,0.75,70862);
INSERT INTO `creature_template_difficulty` (`Entry`,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,`LootID`,`PickPocketLootID`,`SkinLootID`,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild`) SELECT 256797,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,1.0,1.0,`ArmorModifier`,`DamageModifier`,339857,0,0,`TypeFlags3`,0,0,0,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,70862 FROM `creature_template_difficulty` WHERE `Entry`=256795 AND `DifficultyID`=0;
INSERT INTO `creature_classic_level` (`entry`,`level_min`,`level_max`) VALUES (256797,42,43);
-- 255906 Snargl: copy of 255769 Redridge Brute (gnoll), level 40 (quest level 40)
DELETE FROM `creature_template` WHERE `entry`=255906;
DELETE FROM `creature_template_model` WHERE `CreatureID`=255906;
DELETE FROM `creature_template_difficulty` WHERE `Entry`=255906;
DELETE FROM `creature_classic_level` WHERE `entry`=255906;
INSERT INTO `creature_template` (`entry`,`KillCredit1`,`KillCredit2`,`name`,`femaleName`,`subname`,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,`npcflag`,`speed_walk`,`speed_run`,`scale`,`Classification`,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,`family`,`trainer_class`,`type`,`VehicleId`,`AIName`,`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,`ScriptName`,`StringId`,`VerifiedBuild`) SELECT 255906,0,0,'Snargl',`femaleName`,NULL,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,0,`speed_walk`,`speed_run`,`scale`,1,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,1,`unit_flags`,`unit_flags2`,`unit_flags3`,0,`trainer_class`,7,`VehicleId`,'',`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,'',`StringId`,70862 FROM `creature_template` WHERE `entry`=255769;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES (255906,0,376,1.15,1.0,70862);
INSERT INTO `creature_template_difficulty` (`Entry`,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,`LootID`,`PickPocketLootID`,`SkinLootID`,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild`) SELECT 255906,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,3.0,1.0,`ArmorModifier`,`DamageModifier`,338943,0,0,`TypeFlags3`,0,0,0,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,70862 FROM `creature_template_difficulty` WHERE `Entry`=255769 AND `DifficultyID`=0;
INSERT INTO `creature_classic_level` (`entry`,`level_min`,`level_max`) VALUES (255906,40,40);

-- Sergeant Riftan gives Alliance quests but was on faction 7 (anyone could attack him); Cedric Dalton was level 1.
UPDATE `creature_template` SET `faction`=12 WHERE `entry`=258023 AND `faction`=7;
UPDATE `creature_classic_level` SET `level_min`=40, `level_max`=40 WHERE `entry`=259353 AND `level_max`=1;

-- B. 645166 WANTED! (Farholde Keep): starts 95912 and 95991. Template exactly as the cache has it.
DELETE FROM `gameobject_template` WHERE `entry`=645166;
DELETE FROM `gameobject_template_addon` WHERE `entry`=645166;
INSERT INTO `gameobject_template` (`entry`,`type`,`displayId`,`name`,`IconName`,`castBarCaption`,`unk1`,`size`,`Data0`,`Data1`,`Data2`,`Data3`,`Data4`,`Data5`,`Data6`,`Data7`,`Data8`,`Data9`,`Data10`,`Data11`,`Data12`,`Data13`,`Data14`,`Data15`,`Data16`,`Data17`,`Data18`,`Data19`,`Data20`,`Data21`,`Data22`,`Data23`,`Data24`,`Data25`,`Data26`,`Data27`,`Data28`,`Data29`,`Data30`,`Data31`,`Data32`,`Data33`,`Data34`,`ContentTuningId`,`RequiredLevel`,`VerifiedBuild`) VALUES (645166,2,17,'WANTED!','','','',1.0,0,29789,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70862);

-- C. Spawns
-- Ghostmaw: Questie map point 49.0,75.0 (he walks the river, Questie has 64 points); height from our spawn 0 yd away; 15 min respawn, wanders 30 yd.
-- Eastsea Buccaneer x8 and Musketeer x8: within 55 yd of our Eastsea Raider (-7359.1, -5703.8), his height + 2. No spot data for them at all.
-- Twilight Corrupter x6: 12 yd from each of our 3 Fanatics and 3 Champions, their height + 2. The quest wants 8 of each and we
-- had only 3 Fanatics and 3 Champions, so each of them also gets a second one 12 yd away (now 6 of each kind).
-- Snargl: middle of our Redridge gnoll camp (-8472, -3367), height from our spawn 0 yd away; 10 min respawn. Bristle Hills itself has no spot in any data.
-- WANTED! board: where the player stood to take both quests (map 65.8,80.9), height of our NPC 17 yd away.
DELETE FROM `creature` WHERE `guid` BETWEEN 47221000 AND 47221099;
INSERT INTO `creature` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`modelid`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`currentwaypoint`,`curHealthPct`,`MovementType`,`npcflag`,`unit_flags`,`unit_flags2`,`unit_flags3`,`ScriptName`,`StringId`,`VerifiedBuild`) VALUES
(47221000,263380,0,0,0,'0',0,0,0,-1,0,0,-8892.20,-4268.19,26.82,2.99,900,30,0,100,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47221001,257901,0,0,0,'0',0,0,0,-1,0,0,-7337.90,-5702.95,64.16,4.64,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47221002,257910,0,0,0,'0',0,0,0,-1,0,0,-7321.20,-5690.81,64.16,0.57,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47221003,257901,0,0,0,'0',0,0,0,-1,0,0,-7334.99,-5680.43,64.16,2.79,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47221004,257910,0,0,0,'0',0,0,0,-1,0,0,-7348.09,-5665.30,64.16,5.65,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47221005,257901,0,0,0,'0',0,0,0,-1,0,0,-7362.94,-5671.60,64.16,4.24,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47221006,257910,0,0,0,'0',0,0,0,-1,0,0,-7367.00,-5683.54,64.16,2.03,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47221007,257901,0,0,0,'0',0,0,0,-1,0,0,-7372.88,-5691.97,64.16,2.15,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47221008,257910,0,0,0,'0',0,0,0,-1,0,0,-7377.19,-5693.74,64.16,4.11,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47221009,257901,0,0,0,'0',0,0,0,-1,0,0,-7399.45,-5703.30,64.16,2.17,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47221010,257910,0,0,0,'0',0,0,0,-1,0,0,-7397.77,-5719.15,64.16,1.83,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47221011,257901,0,0,0,'0',0,0,0,-1,0,0,-7370.76,-5719.44,64.16,4.25,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47221012,257910,0,0,0,'0',0,0,0,-1,0,0,-7368.86,-5724.54,64.16,3.21,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47221013,257901,0,0,0,'0',0,0,0,-1,0,0,-7358.87,-5729.53,64.16,3.68,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47221014,257910,0,0,0,'0',0,0,0,-1,0,0,-7342.86,-5738.83,64.16,2.64,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47221015,257901,0,0,0,'0',0,0,0,-1,0,0,-7344.91,-5714.89,64.16,4.59,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47221016,257910,0,0,0,'0',0,0,0,-1,0,0,-7329.49,-5718.13,64.16,5.11,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47221017,256797,0,0,0,'0',0,0,0,-1,0,0,-8043.57,-3365.23,169.36,2.48,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47221018,256795,0,0,0,'0',0,0,0,-1,0,0,-8021.77,-3372.96,169.36,4.59,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47221019,256797,0,0,0,'0',0,0,0,-1,0,0,-7978.83,-3396.70,164.81,0.90,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47221020,256795,0,0,0,'0',0,0,0,-1,0,0,-7955.94,-3400.00,164.81,5.67,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47221021,256797,0,0,0,'0',0,0,0,-1,0,0,-7677.70,-3471.80,177.79,3.48,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47221022,256795,0,0,0,'0',0,0,0,-1,0,0,-7657.92,-3459.83,177.79,3.40,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47221023,256797,0,0,0,'0',0,0,0,-1,0,0,-7992.57,-3337.23,164.94,2.75,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47221024,256799,0,0,0,'0',0,0,0,-1,0,0,-7999.88,-3315.29,164.94,0.34,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47221025,256797,0,0,0,'0',0,0,0,-1,0,0,-8065.60,-3341.08,167.09,0.59,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47221026,256799,0,0,0,'0',0,0,0,-1,0,0,-8084.41,-3327.64,167.09,1.92,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47221027,256797,0,0,0,'0',0,0,0,-1,0,0,-8014.88,-3393.24,167.22,0.98,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47221028,256799,0,0,0,'0',0,0,0,-1,0,0,-7993.89,-3383.53,167.22,1.90,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47221029,255906,0,0,0,'0',0,0,0,-1,0,0,-8472.19,-3367.48,174.87,4.28,600,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,0);
DELETE FROM `gameobject` WHERE `guid` BETWEEN 47231000 AND 47231009;
INSERT INTO `gameobject` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`,`ScriptName`,`StringId`,`VerifiedBuild`) VALUES
(47231000,645166,0,0,0,'0',0,0,0,-1,-9083.02,-5080.58,38.58,0,0,0,0,1,300,255,1,'',NULL,0);

-- D. Quest drops (quest-only)
UPDATE `creature_template_difficulty` SET `LootID`=263071 WHERE `Entry`=263071 AND `DifficultyID`=0 AND `LootID`=0;
DELETE FROM `creature_loot_template` WHERE `Entry`=263071 AND `ItemType`=0 AND `Item`=271425;
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES (263071,0,271425,100,1,1,0,1,1,'Blademaster Kaijo - quest 95912');
UPDATE `creature_template_difficulty` SET `LootID`=263380 WHERE `Entry`=263380 AND `DifficultyID`=0 AND `LootID`=0;
DELETE FROM `creature_loot_template` WHERE `Entry`=263380 AND `ItemType`=0 AND `Item`=271828;
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES (263380,0,271828,100,1,1,0,1,1,'Ghostmaw - quest 95991');
UPDATE `creature_template_difficulty` SET `LootID`=255906 WHERE `Entry`=255906 AND `DifficultyID`=0 AND `LootID`=0;
DELETE FROM `creature_loot_template` WHERE `Entry`=255906 AND `ItemType`=0 AND `Item`=265357;
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES (255906,0,265357,100,1,1,0,1,1,'Snargl - quest 94679');
UPDATE `creature_template_difficulty` SET `LootID`=257901 WHERE `Entry`=257901 AND `DifficultyID`=0 AND `LootID`=0;
DELETE FROM `creature_loot_template` WHERE `Entry`=257901 AND `ItemType`=0 AND `Item`=268796;
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES (257901,0,268796,40,1,1,0,1,1,'Eastsea Buccaneer - quest 95288 (chance ESTIMATED)');
UPDATE `creature_template_difficulty` SET `LootID`=257910 WHERE `Entry`=257910 AND `DifficultyID`=0 AND `LootID`=0;
DELETE FROM `creature_loot_template` WHERE `Entry`=257910 AND `ItemType`=0 AND `Item`=268796;
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES (257910,0,268796,40,1,1,0,1,1,'Eastsea Musketeer - quest 95288 (chance ESTIMATED)');

-- E. Quests
-- 95912 WANTED: Blademaster Kaijo: from the WANTED! board in Farholde Keep (trace + Questie), back to Ember Bladewhisper (log line)
DELETE FROM `quest_template` WHERE `ID`=95912;
INSERT INTO `quest_template` (`ID`,`QuestType`,`LogTitle`,`VerifiedBuild`) VALUES (95912,2,'WANTED: Blademaster Kaijo',-1);
DELETE FROM `quest_classic_level` WHERE `ID`=95912;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (95912,0,0,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=95912;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (95912,0,0);
DELETE FROM `quest_template_addon` WHERE `ID`=95912;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (95912,0,0,0);
DELETE FROM `quest_offer_reward` WHERE `ID`=95912;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (95912,'Well done, $n. Thank you.',-1);
DELETE FROM `creature_queststarter` WHERE `quest`=95912;
DELETE FROM `gameobject_queststarter` WHERE `quest`=95912;
DELETE FROM `creature_questender` WHERE `quest`=95912;
INSERT INTO `gameobject_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (645166,95912,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (260080,95912,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (260080);
UPDATE `quest_template` SET `QuestDescription`='With each orc we cut down, another takes its place--somehow even more capable than the last. A skilled warrior must be training this new generation. If we are to make a dent in their numbers, we need to ensure they don''t have a chance to recoup their losses.\n\nIf any soldier is brave enough, it''s imperative we find this blademaster and put an end to him. He is likely at the trainee camp north of the Horde outpost even now. Kill him, take his banner, and destroy the orcs'' morale.\n\n-- Ember Bladewhisper', `LogDescription`='Bring the Blademaster''s Banner to Ember Bladewhisper in Farholde Keep.', `AreaDescription`='', `QuestCompletionLog`='', `LogTitle`='WANTED: Blademaster Kaijo', `QuestSortID`=16591, `QuestInfoID`=1, `SuggestedGroupNum`=0, `RewardNextQuest`=0, `RewardXPDifficulty`=6, `RewardXPMultiplier`=1.0, `RewardMoneyDifficulty`=0, `RewardMoneyMultiplier`=1.0, `RewardSpell`=0, `StartItem`=0, `Flags`=36700168, `FlagsEx`=0, `AllowableRaces`=6130900294268439629, `TimeAllowed`=0, `VerifiedBuild`=70862, `RewardItem1`=0, `RewardAmount1`=0, `ItemDrop1`=0, `ItemDropQuantity1`=0, `RewardItem2`=0, `RewardAmount2`=0, `ItemDrop2`=0, `ItemDropQuantity2`=0, `RewardItem3`=0, `RewardAmount3`=0, `ItemDrop3`=0, `ItemDropQuantity3`=0, `RewardItem4`=0, `RewardAmount4`=0, `ItemDrop4`=0, `ItemDropQuantity4`=0, `RewardChoiceItemID1`=276102, `RewardChoiceItemQuantity1`=1, `RewardChoiceItemDisplayID1`=0, `RewardChoiceItemID2`=287072, `RewardChoiceItemQuantity2`=1, `RewardChoiceItemDisplayID2`=0, `RewardChoiceItemID3`=0, `RewardChoiceItemQuantity3`=0, `RewardChoiceItemDisplayID3`=0, `RewardChoiceItemID4`=0, `RewardChoiceItemQuantity4`=0, `RewardChoiceItemDisplayID4`=0, `RewardChoiceItemID5`=0, `RewardChoiceItemQuantity5`=0, `RewardChoiceItemDisplayID5`=0, `RewardChoiceItemID6`=0, `RewardChoiceItemQuantity6`=0, `RewardChoiceItemDisplayID6`=0, `RewardFactionID1`=2826, `RewardFactionValue1`=6, `RewardFactionOverride1`=0, `RewardFactionCapIn1`=7, `RewardFactionID2`=0, `RewardFactionValue2`=0, `RewardFactionOverride2`=0, `RewardFactionCapIn2`=7, `RewardFactionID3`=0, `RewardFactionValue3`=0, `RewardFactionOverride3`=0, `RewardFactionCapIn3`=7, `RewardFactionID4`=0, `RewardFactionValue4`=0, `RewardFactionOverride4`=0, `RewardFactionCapIn4`=7, `RewardFactionID5`=0, `RewardFactionValue5`=0, `RewardFactionOverride5`=0, `RewardFactionCapIn5`=7 WHERE `ID`=95912 AND `VerifiedBuild`=-1;
DELETE FROM `quest_objectives` WHERE `QuestID`=95912;
DELETE FROM `quest_objectives` WHERE `ID` IN (471652);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (471652,95912,1,0,1,271425,1,0,32,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_classic_money` WHERE `ID`=95912;
UPDATE `quest_classic_level` SET `QuestLevel`=42, `MinLevel`=38 WHERE `ID`=95912;
UPDATE `quest_template_classic_level` SET `LevelType`=2, `QuestLevel`=42 WHERE `ID`=95912;
UPDATE `quest_template` SET `QuestType`=2 WHERE `ID`=95912;

-- 95991 WANTED: Ghostmaw: from the WANTED! board in Farholde Keep (trace + Questie), back to Wheatley (Questie + log line)
DELETE FROM `quest_template` WHERE `ID`=95991;
INSERT INTO `quest_template` (`ID`,`QuestType`,`LogTitle`,`VerifiedBuild`) VALUES (95991,2,'WANTED: Ghostmaw',-1);
DELETE FROM `quest_classic_level` WHERE `ID`=95991;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (95991,0,0,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=95991;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (95991,0,0);
DELETE FROM `quest_template_addon` WHERE `ID`=95991;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (95991,0,0,0);
DELETE FROM `quest_offer_reward` WHERE `ID`=95991;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (95991,'Well done, $n. Thank you.',-1);
DELETE FROM `creature_queststarter` WHERE `quest`=95991;
DELETE FROM `gameobject_queststarter` WHERE `quest`=95991;
DELETE FROM `creature_questender` WHERE `quest`=95991;
INSERT INTO `gameobject_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (645166,95991,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (259859,95991,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (259859);
UPDATE `quest_template` SET `QuestDescription`='It''s dangerous enough transportin'' horses ''round the glades without talk of some mysterious phantom crocolisk snappin'' up my prized ponies! They say this one must be at least three times as big as a regular croc. And twice as mean!\n\nI ain''t a gamblin'' man. I ain''t takin'' no chances with my horses till someone puts this good-fer-nothin'' monster down. If anyone finds this varmint along the rivers, bring me its tooth so''s I know it''s safe to travel again. Till then, I''m stayin'' put!\n\n-- Wheatley', `LogDescription`='Bring Ghostmaw''s Fang to Wheatley in Farholde Keep.', `AreaDescription`='', `QuestCompletionLog`='', `LogTitle`='WANTED: Ghostmaw', `QuestSortID`=16591, `QuestInfoID`=1, `SuggestedGroupNum`=0, `RewardNextQuest`=0, `RewardXPDifficulty`=6, `RewardXPMultiplier`=1.0, `RewardMoneyDifficulty`=0, `RewardMoneyMultiplier`=1.0, `RewardSpell`=0, `StartItem`=0, `Flags`=36700168, `FlagsEx`=0, `AllowableRaces`=6130900294268439629, `TimeAllowed`=0, `VerifiedBuild`=70862, `RewardItem1`=274962, `RewardAmount1`=1, `ItemDrop1`=0, `ItemDropQuantity1`=0, `RewardItem2`=0, `RewardAmount2`=0, `ItemDrop2`=0, `ItemDropQuantity2`=0, `RewardItem3`=0, `RewardAmount3`=0, `ItemDrop3`=0, `ItemDropQuantity3`=0, `RewardItem4`=0, `RewardAmount4`=0, `ItemDrop4`=0, `ItemDropQuantity4`=0, `RewardChoiceItemID1`=0, `RewardChoiceItemQuantity1`=0, `RewardChoiceItemDisplayID1`=0, `RewardChoiceItemID2`=0, `RewardChoiceItemQuantity2`=0, `RewardChoiceItemDisplayID2`=0, `RewardChoiceItemID3`=0, `RewardChoiceItemQuantity3`=0, `RewardChoiceItemDisplayID3`=0, `RewardChoiceItemID4`=0, `RewardChoiceItemQuantity4`=0, `RewardChoiceItemDisplayID4`=0, `RewardChoiceItemID5`=0, `RewardChoiceItemQuantity5`=0, `RewardChoiceItemDisplayID5`=0, `RewardChoiceItemID6`=0, `RewardChoiceItemQuantity6`=0, `RewardChoiceItemDisplayID6`=0, `RewardFactionID1`=2826, `RewardFactionValue1`=6, `RewardFactionOverride1`=0, `RewardFactionCapIn1`=7, `RewardFactionID2`=0, `RewardFactionValue2`=0, `RewardFactionOverride2`=0, `RewardFactionCapIn2`=7, `RewardFactionID3`=0, `RewardFactionValue3`=0, `RewardFactionOverride3`=0, `RewardFactionCapIn3`=7, `RewardFactionID4`=0, `RewardFactionValue4`=0, `RewardFactionOverride4`=0, `RewardFactionCapIn4`=7, `RewardFactionID5`=0, `RewardFactionValue5`=0, `RewardFactionOverride5`=0, `RewardFactionCapIn5`=7 WHERE `ID`=95991 AND `VerifiedBuild`=-1;
DELETE FROM `quest_objectives` WHERE `QuestID`=95991;
DELETE FROM `quest_objectives` WHERE `ID` IN (471783);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (471783,95991,1,0,0,271828,1,0,32,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_classic_money` WHERE `ID`=95991;
UPDATE `quest_classic_level` SET `QuestLevel`=42, `MinLevel`=38 WHERE `ID`=95991;
UPDATE `quest_template_classic_level` SET `LevelType`=2, `QuestLevel`=42 WHERE `ID`=95991;
UPDATE `quest_template` SET `QuestType`=2 WHERE `ID`=95991;

-- 95119 Unfriendly Competition: giver and turn-in Wharfmaster Whizzbuzz (trace + Questie)
DELETE FROM `quest_template` WHERE `ID`=95119;
INSERT INTO `quest_template` (`ID`,`QuestType`,`LogTitle`,`VerifiedBuild`) VALUES (95119,2,'Unfriendly Competition',-1);
DELETE FROM `quest_classic_level` WHERE `ID`=95119;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (95119,0,0,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=95119;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (95119,0,0);
DELETE FROM `quest_template_addon` WHERE `ID`=95119;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (95119,0,0,0);
DELETE FROM `quest_offer_reward` WHERE `ID`=95119;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (95119,'Well done, $n. Thank you.',-1);
DELETE FROM `creature_queststarter` WHERE `quest`=95119;
DELETE FROM `gameobject_queststarter` WHERE `quest`=95119;
DELETE FROM `creature_questender` WHERE `quest`=95119;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (259961,95119,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (259961,95119,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (259961);
UPDATE `quest_template` SET `QuestDescription`='You know the Eastsea crew? Real unsavory bunch, and that''s coming from the guy running a place called "Powderfuse Port".\n\nThey''re destructive, bloodthirsty, and, worst of all, cutting into my bottom line. They steal cargo, and sink ships, both of which end up costing me the big bucks!\n\nWe gotta take down their whole operation, starting with the ones closest to here. You can find ''em along the beaches to the north.', `LogDescription`='Kill $1oa Eastsea Buccaneers and $2oa Eastsea Musketeers along the Eastwind Shore, north of Powderfuse Port.', `AreaDescription`='', `QuestCompletionLog`='', `LogTitle`='Unfriendly Competition', `QuestSortID`=16591, `QuestInfoID`=0, `SuggestedGroupNum`=0, `RewardNextQuest`=0, `RewardXPDifficulty`=5, `RewardXPMultiplier`=1.0, `RewardMoneyDifficulty`=5, `RewardMoneyMultiplier`=1.0, `RewardSpell`=0, `StartItem`=0, `Flags`=36700168, `FlagsEx`=0, `AllowableRaces`=18446744073709551615, `TimeAllowed`=0, `VerifiedBuild`=70862, `RewardItem1`=0, `RewardAmount1`=0, `ItemDrop1`=0, `ItemDropQuantity1`=0, `RewardItem2`=0, `RewardAmount2`=0, `ItemDrop2`=0, `ItemDropQuantity2`=0, `RewardItem3`=0, `RewardAmount3`=0, `ItemDrop3`=0, `ItemDropQuantity3`=0, `RewardItem4`=0, `RewardAmount4`=0, `ItemDrop4`=0, `ItemDropQuantity4`=0, `RewardChoiceItemID1`=0, `RewardChoiceItemQuantity1`=0, `RewardChoiceItemDisplayID1`=0, `RewardChoiceItemID2`=0, `RewardChoiceItemQuantity2`=0, `RewardChoiceItemDisplayID2`=0, `RewardChoiceItemID3`=0, `RewardChoiceItemQuantity3`=0, `RewardChoiceItemDisplayID3`=0, `RewardChoiceItemID4`=0, `RewardChoiceItemQuantity4`=0, `RewardChoiceItemDisplayID4`=0, `RewardChoiceItemID5`=0, `RewardChoiceItemQuantity5`=0, `RewardChoiceItemDisplayID5`=0, `RewardChoiceItemID6`=0, `RewardChoiceItemQuantity6`=0, `RewardChoiceItemDisplayID6`=0, `RewardFactionID1`=2827, `RewardFactionValue1`=5, `RewardFactionOverride1`=0, `RewardFactionCapIn1`=7, `RewardFactionID2`=0, `RewardFactionValue2`=0, `RewardFactionOverride2`=0, `RewardFactionCapIn2`=7, `RewardFactionID3`=0, `RewardFactionValue3`=0, `RewardFactionOverride3`=0, `RewardFactionCapIn3`=7, `RewardFactionID4`=0, `RewardFactionValue4`=0, `RewardFactionOverride4`=0, `RewardFactionCapIn4`=7, `RewardFactionID5`=0, `RewardFactionValue5`=0, `RewardFactionOverride5`=0, `RewardFactionCapIn5`=7 WHERE `ID`=95119 AND `VerifiedBuild`=-1;
DELETE FROM `quest_objectives` WHERE `QuestID`=95119;
DELETE FROM `quest_objectives` WHERE `ID` IN (469850,469851);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (469850,95119,0,0,0,257901,10,1413,32,0,0.0,0,1,NULL,70862),(469851,95119,0,1,1,257910,10,1,32,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_classic_money` WHERE `ID`=95119;
INSERT INTO `quest_classic_money` (`ID`,`RewardMoney`,`RewardMoneyMaxLevel`) VALUES (95119,6000,0);
UPDATE `quest_classic_level` SET `QuestLevel`=43, `MinLevel`=38 WHERE `ID`=95119;
UPDATE `quest_template_classic_level` SET `LevelType`=2, `QuestLevel`=43 WHERE `ID`=95119;
UPDATE `quest_template` SET `QuestType`=2 WHERE `ID`=95119;

-- 95288 Smuggler's Paradise: giver Friz Frazzlespark (trace), turn-in Friz (log line)
DELETE FROM `quest_template` WHERE `ID`=95288;
INSERT INTO `quest_template` (`ID`,`QuestType`,`LogTitle`,`VerifiedBuild`) VALUES (95288,2,'Smuggler''s Paradise',-1);
DELETE FROM `quest_classic_level` WHERE `ID`=95288;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (95288,0,0,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=95288;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (95288,0,0);
DELETE FROM `quest_template_addon` WHERE `ID`=95288;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (95288,0,0,0);
DELETE FROM `quest_offer_reward` WHERE `ID`=95288;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (95288,'Well done, $n. Thank you.',-1);
DELETE FROM `creature_queststarter` WHERE `quest`=95288;
DELETE FROM `gameobject_queststarter` WHERE `quest`=95288;
DELETE FROM `creature_questender` WHERE `quest`=95288;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (255897,95288,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (255897,95288,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (255897);
UPDATE `quest_template` SET `QuestDescription`='You look like you''re in the habit of doing whatever other people tell ya. I like that in a lackey. With a little trainin'' you could be a great asset. Let''s put that to the test.\n\nRecently some of those Eastsea pirates made off with a shipment of MY stolen goods. That''s bad sportsmanship. I stole them first!\n\nLesson one, lackey. Never backstab a backstabber. Go show them that they messed with the wrong goblin and take back my stuff. With force, preferably.', `LogDescription`='Collect $1oa Smuggled Goods for Friz Frazzlespark at Powderfuse Port.', `AreaDescription`='', `QuestCompletionLog`='', `LogTitle`='Smuggler''s Paradise', `QuestSortID`=16591, `QuestInfoID`=0, `SuggestedGroupNum`=0, `RewardNextQuest`=95293, `RewardXPDifficulty`=5, `RewardXPMultiplier`=1.0, `RewardMoneyDifficulty`=0, `RewardMoneyMultiplier`=1.0, `RewardSpell`=0, `StartItem`=0, `Flags`=36700168, `FlagsEx`=0, `AllowableRaces`=18446744073709551615, `TimeAllowed`=0, `VerifiedBuild`=70862, `RewardItem1`=0, `RewardAmount1`=0, `ItemDrop1`=0, `ItemDropQuantity1`=0, `RewardItem2`=0, `RewardAmount2`=0, `ItemDrop2`=0, `ItemDropQuantity2`=0, `RewardItem3`=0, `RewardAmount3`=0, `ItemDrop3`=0, `ItemDropQuantity3`=0, `RewardItem4`=0, `RewardAmount4`=0, `ItemDrop4`=0, `ItemDropQuantity4`=0, `RewardChoiceItemID1`=0, `RewardChoiceItemQuantity1`=0, `RewardChoiceItemDisplayID1`=0, `RewardChoiceItemID2`=0, `RewardChoiceItemQuantity2`=0, `RewardChoiceItemDisplayID2`=0, `RewardChoiceItemID3`=0, `RewardChoiceItemQuantity3`=0, `RewardChoiceItemDisplayID3`=0, `RewardChoiceItemID4`=0, `RewardChoiceItemQuantity4`=0, `RewardChoiceItemDisplayID4`=0, `RewardChoiceItemID5`=0, `RewardChoiceItemQuantity5`=0, `RewardChoiceItemDisplayID5`=0, `RewardChoiceItemID6`=0, `RewardChoiceItemQuantity6`=0, `RewardChoiceItemDisplayID6`=0, `RewardFactionID1`=2827, `RewardFactionValue1`=5, `RewardFactionOverride1`=0, `RewardFactionCapIn1`=7, `RewardFactionID2`=0, `RewardFactionValue2`=0, `RewardFactionOverride2`=0, `RewardFactionCapIn2`=7, `RewardFactionID3`=0, `RewardFactionValue3`=0, `RewardFactionOverride3`=0, `RewardFactionCapIn3`=7, `RewardFactionID4`=0, `RewardFactionValue4`=0, `RewardFactionOverride4`=0, `RewardFactionCapIn4`=7, `RewardFactionID5`=0, `RewardFactionValue5`=0, `RewardFactionOverride5`=0, `RewardFactionCapIn5`=7 WHERE `ID`=95288 AND `VerifiedBuild`=-1;
DELETE FROM `quest_objectives` WHERE `QuestID`=95288;
DELETE FROM `quest_objectives` WHERE `ID` IN (470232);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (470232,95288,1,0,1,268796,8,0,32,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_classic_money` WHERE `ID`=95288;
UPDATE `quest_classic_level` SET `QuestLevel`=43, `MinLevel`=38 WHERE `ID`=95288;
UPDATE `quest_template_classic_level` SET `LevelType`=2, `QuestLevel`=43 WHERE `ID`=95288;
UPDATE `quest_template` SET `QuestType`=2 WHERE `ID`=95288;

-- 94889 Carving a Path: giver Sergeant Riftan (trace), turn-in Riftan (log line)
DELETE FROM `quest_template` WHERE `ID`=94889;
INSERT INTO `quest_template` (`ID`,`QuestType`,`LogTitle`,`VerifiedBuild`) VALUES (94889,2,'Carving a Path',-1);
DELETE FROM `quest_classic_level` WHERE `ID`=94889;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (94889,0,0,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=94889;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (94889,0,0);
DELETE FROM `quest_template_addon` WHERE `ID`=94889;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (94889,0,0,0);
DELETE FROM `quest_offer_reward` WHERE `ID`=94889;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (94889,'Well done, $n. Thank you.',-1);
DELETE FROM `creature_queststarter` WHERE `quest`=94889;
DELETE FROM `gameobject_queststarter` WHERE `quest`=94889;
DELETE FROM `creature_questender` WHERE `quest`=94889;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (258023,94889,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (258023,94889,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (258023);
UPDATE `quest_template` SET `QuestDescription`='It seems every day the Twilight''s Hammer are drawing more into their order, while we haven''t heard so much as a word from Stormwind. Sooner or later we''ll be overrun.\n\nI don''t care what a bureaucrat like Amberhall says. I''m not abandoning this position while an army amasses in our own homeland. If we leave now, we''ll only have to deal with these cultists again later. Only next time, on our doorstep.\n\nIf your orders are to babysit me anyway, at least make yourself useful and help me cull these zealots.', `LogDescription`='Defeat $1oa Twilight Fanatics, $2oa Twilight Corrupters, and $3oa Twilight Champions within the Twilight''s Shroud for Sergeant Riftan.', `AreaDescription`='', `QuestCompletionLog`='', `LogTitle`='Carving a Path', `QuestSortID`=16591, `QuestInfoID`=0, `SuggestedGroupNum`=0, `RewardNextQuest`=0, `RewardXPDifficulty`=5, `RewardXPMultiplier`=1.0, `RewardMoneyDifficulty`=3, `RewardMoneyMultiplier`=1.0, `RewardSpell`=0, `StartItem`=0, `Flags`=36700168, `FlagsEx`=0, `AllowableRaces`=6130900294268439629, `TimeAllowed`=0, `VerifiedBuild`=70862, `RewardItem1`=0, `RewardAmount1`=0, `ItemDrop1`=0, `ItemDropQuantity1`=0, `RewardItem2`=0, `RewardAmount2`=0, `ItemDrop2`=0, `ItemDropQuantity2`=0, `RewardItem3`=0, `RewardAmount3`=0, `ItemDrop3`=0, `ItemDropQuantity3`=0, `RewardItem4`=0, `RewardAmount4`=0, `ItemDrop4`=0, `ItemDropQuantity4`=0, `RewardChoiceItemID1`=0, `RewardChoiceItemQuantity1`=0, `RewardChoiceItemDisplayID1`=0, `RewardChoiceItemID2`=0, `RewardChoiceItemQuantity2`=0, `RewardChoiceItemDisplayID2`=0, `RewardChoiceItemID3`=0, `RewardChoiceItemQuantity3`=0, `RewardChoiceItemDisplayID3`=0, `RewardChoiceItemID4`=0, `RewardChoiceItemQuantity4`=0, `RewardChoiceItemDisplayID4`=0, `RewardChoiceItemID5`=0, `RewardChoiceItemQuantity5`=0, `RewardChoiceItemDisplayID5`=0, `RewardChoiceItemID6`=0, `RewardChoiceItemQuantity6`=0, `RewardChoiceItemDisplayID6`=0, `RewardFactionID1`=2826, `RewardFactionValue1`=5, `RewardFactionOverride1`=0, `RewardFactionCapIn1`=7, `RewardFactionID2`=0, `RewardFactionValue2`=0, `RewardFactionOverride2`=0, `RewardFactionCapIn2`=7, `RewardFactionID3`=0, `RewardFactionValue3`=0, `RewardFactionOverride3`=0, `RewardFactionCapIn3`=7, `RewardFactionID4`=0, `RewardFactionValue4`=0, `RewardFactionOverride4`=0, `RewardFactionCapIn4`=7, `RewardFactionID5`=0, `RewardFactionValue5`=0, `RewardFactionOverride5`=0, `RewardFactionCapIn5`=7 WHERE `ID`=94889 AND `VerifiedBuild`=-1;
DELETE FROM `quest_objectives` WHERE `QuestID`=94889;
DELETE FROM `quest_objectives` WHERE `ID` IN (469303,469302,469573);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (469303,94889,0,0,1,256795,8,0,32,0,0.0,0,1,NULL,70862),(469302,94889,0,1,0,256797,8,0,32,0,0.0,0,1,NULL,70862),(469573,94889,0,2,2,256799,8,0,32,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_classic_money` WHERE `ID`=94889;
INSERT INTO `quest_classic_money` (`ID`,`RewardMoney`,`RewardMoneyMaxLevel`) VALUES (94889,3000,0);
UPDATE `quest_classic_level` SET `QuestLevel`=44, `MinLevel`=38 WHERE `ID`=94889;
UPDATE `quest_template_classic_level` SET `LevelType`=2, `QuestLevel`=44 WHERE `ID`=94889;
UPDATE `quest_template` SET `QuestType`=2 WHERE `ID`=94889;

-- 94679 Snargl's Snout: giver Cedric Dalton (trace), turn-in Cedric Dalton (log line)
DELETE FROM `quest_template` WHERE `ID`=94679;
INSERT INTO `quest_template` (`ID`,`QuestType`,`LogTitle`,`VerifiedBuild`) VALUES (94679,2,'Snargl''s Snout',-1);
DELETE FROM `quest_classic_level` WHERE `ID`=94679;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (94679,0,0,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=94679;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (94679,0,0);
DELETE FROM `quest_template_addon` WHERE `ID`=94679;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (94679,0,0,0);
DELETE FROM `quest_offer_reward` WHERE `ID`=94679;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (94679,'Well done, $n. Thank you.',-1);
DELETE FROM `creature_queststarter` WHERE `quest`=94679;
DELETE FROM `gameobject_queststarter` WHERE `quest`=94679;
DELETE FROM `creature_questender` WHERE `quest`=94679;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (259353,94679,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (259353,94679,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (259353);
UPDATE `quest_template` SET `QuestDescription`='Gnolls value strength above all else. By killing their strongest, we not only lessen the threat they pose physically, but also strike fear into their hearts.\n\nIn a den among the Bristle Hills is a notorious gnoll by the name of Snargl. His death would no doubt sow panic among the Redridge''s ranks.\n\nBring me his snout, and be quick about it.', `LogDescription`='Locate Snargl in the Bristle Hills, kill him, and bring his snout to Cedric Dalton at Southern Watch.', `AreaDescription`='', `QuestCompletionLog`='', `LogTitle`='Snargl''s Snout', `QuestSortID`=16591, `QuestInfoID`=1, `SuggestedGroupNum`=0, `RewardNextQuest`=0, `RewardXPDifficulty`=5, `RewardXPMultiplier`=1.0, `RewardMoneyDifficulty`=0, `RewardMoneyMultiplier`=1.0, `RewardSpell`=0, `StartItem`=0, `Flags`=36700168, `FlagsEx`=0, `AllowableRaces`=6130900294268439629, `TimeAllowed`=0, `VerifiedBuild`=70862, `RewardItem1`=0, `RewardAmount1`=0, `ItemDrop1`=0, `ItemDropQuantity1`=0, `RewardItem2`=0, `RewardAmount2`=0, `ItemDrop2`=0, `ItemDropQuantity2`=0, `RewardItem3`=0, `RewardAmount3`=0, `ItemDrop3`=0, `ItemDropQuantity3`=0, `RewardItem4`=0, `RewardAmount4`=0, `ItemDrop4`=0, `ItemDropQuantity4`=0, `RewardChoiceItemID1`=274938, `RewardChoiceItemQuantity1`=1, `RewardChoiceItemDisplayID1`=0, `RewardChoiceItemID2`=274939, `RewardChoiceItemQuantity2`=1, `RewardChoiceItemDisplayID2`=0, `RewardChoiceItemID3`=274940, `RewardChoiceItemQuantity3`=1, `RewardChoiceItemDisplayID3`=0, `RewardChoiceItemID4`=274941, `RewardChoiceItemQuantity4`=1, `RewardChoiceItemDisplayID4`=0, `RewardChoiceItemID5`=0, `RewardChoiceItemQuantity5`=0, `RewardChoiceItemDisplayID5`=0, `RewardChoiceItemID6`=0, `RewardChoiceItemQuantity6`=0, `RewardChoiceItemDisplayID6`=0, `RewardFactionID1`=2826, `RewardFactionValue1`=5, `RewardFactionOverride1`=0, `RewardFactionCapIn1`=7, `RewardFactionID2`=0, `RewardFactionValue2`=0, `RewardFactionOverride2`=0, `RewardFactionCapIn2`=7, `RewardFactionID3`=0, `RewardFactionValue3`=0, `RewardFactionOverride3`=0, `RewardFactionCapIn3`=7, `RewardFactionID4`=0, `RewardFactionValue4`=0, `RewardFactionOverride4`=0, `RewardFactionCapIn4`=7, `RewardFactionID5`=0, `RewardFactionValue5`=0, `RewardFactionOverride5`=0, `RewardFactionCapIn5`=7 WHERE `ID`=94679 AND `VerifiedBuild`=-1;
DELETE FROM `quest_objectives` WHERE `QuestID`=94679;
DELETE FROM `quest_objectives` WHERE `ID` IN (469083);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (469083,94679,1,0,3,265357,1,1702125924,32,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_classic_money` WHERE `ID`=94679;
UPDATE `quest_classic_level` SET `QuestLevel`=40, `MinLevel`=36 WHERE `ID`=94679;
UPDATE `quest_template_classic_level` SET `LevelType`=2, `QuestLevel`=40 WHERE `ID`=94679;
UPDATE `quest_template` SET `QuestType`=2 WHERE `ID`=94679;

