-- WoW Forever: Krol'dok Stronghold (map 3039, Riverglades, levels 40-45) made playable: instance, way in and out, boss and
-- trash spawns, trash abilities, boss loot. The fights are in classic_instance_kroldok_stronghold.cpp.
-- No sniff of this dungeon exists and no player cache has its trash yet. What is real data and what is a guess:
--  * real (70862 client files): the map and its navmesh (every spawn sits on the navmesh piece that holds the whole
--    stronghold), the encounter list, the spells. Boss loot item ids exist in our hotfixes. Bossman Toof's template is
--    from a player's cache (live realm 70862); the five bosses' templates came in 2026_11_10_81.
--  * from the public dungeon guides: the entrance (cave at Riverglades 37.5, 37.2), the route uphill to Mugg at the top,
--    the boss order, Ashka's wolves, Toof's shamans, Mugg's Centurions, the trash kinds (ogre brutes and peons, Twilight
--    cultists), the boss loot lists.
--  * PLACEHOLDER (entries 8110000-8110005): the trash. Names, models (vanilla ogre, Twilight and wolf models), levels and
--    abilities are our guesses. Swap them for the live entries as soon as a cache or sniff shows them.
--  * INFERRED: every spawn spot (bosses spread along the walking route from the cave to the top, trash every ~48 yd
--    between them), levels, drop chances, gold, timers, the landing and exit spots.
-- Spawn guids 48110000-48110199. Safe to run again.

DELETE FROM `instance_template` WHERE `map` = 3039;
INSERT INTO `instance_template` (`map`,`parent`,`script`) VALUES
(3039,0,'classic_instance_kroldok_stronghold');

-- placeholder trash (PLACEHOLDER: names, models and stats are guesses) and Bossman Toof (player's cache; level 44 INFERRED)
DELETE FROM `creature_template` WHERE `entry` IN (8110000,8110001,8110002,8110003,8110004,8110005,264773);
INSERT INTO `creature_template` (`entry`,`KillCredit1`,`KillCredit2`,`name`,`femaleName`,`subname`,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,`npcflag`,`speed_walk`,`speed_run`,`scale`,`Classification`,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,`family`,`trainer_class`,`type`,`VehicleId`,`AIName`,`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,`ScriptName`,`StringId`,`VerifiedBuild`) VALUES
(8110000,0,0,'Krol''dok Brute',NULL,NULL,NULL,NULL,0,0,16,0,1.0,1.14286,1.0,1,0,2000,2000,1.0,1.0,1,0,2048,0,0,0,7,0,'SmartAI',0,1.0,0,0,0,0,1,0,0,'',NULL,0),
(8110001,0,0,'Krol''dok Peon',NULL,NULL,NULL,NULL,0,0,16,0,1.0,1.14286,1.0,1,0,2000,2000,1.0,1.0,1,0,2048,0,0,0,7,0,'SmartAI',0,1.0,0,0,0,0,1,0,0,'',NULL,0),
(8110002,0,0,'Krol''dok Shaman',NULL,NULL,NULL,NULL,0,0,16,0,1.0,1.14286,1.0,1,0,2000,2000,1.0,1.0,2,0,2048,0,0,0,7,0,'SmartAI',0,1.0,0,0,0,0,1,0,0,'',NULL,0),
(8110003,0,0,'Twilight Cultist',NULL,NULL,NULL,NULL,0,0,16,0,1.0,1.14286,1.0,1,0,2000,2000,1.0,1.0,8,0,2048,0,0,0,7,0,'SmartAI',0,1.0,0,0,0,0,1,0,0,'',NULL,0),
(8110004,0,0,'Krol''dok Centurion',NULL,NULL,NULL,NULL,0,0,16,0,1.0,1.14286,1.0,1,0,2000,2000,1.0,1.0,1,0,2048,0,0,0,7,0,'SmartAI',0,1.0,0,0,0,0,1,0,0,'',NULL,0),
(8110005,0,0,'Krol''dok War Wolf',NULL,NULL,NULL,NULL,0,0,16,0,1.0,1.14286,1.0,1,0,2000,2000,1.0,1.0,1,0,2048,0,1,0,1,0,'SmartAI',0,1.0,0,0,0,0,1,0,0,'',NULL,0),
(264773,0,0,'Bossman Toof',NULL,NULL,NULL,NULL,0,0,16,0,1.0,1.14286,1.0,1,0,2000,2000,1.0,1.0,1,0,2048,0,0,0,7,0,'',0,1.0,0,0,0,0,1,0,0,'classic_boss_kds_toof',NULL,0);
DELETE FROM `creature_template_difficulty` WHERE `Entry` IN (8110000,8110001,8110002,8110003,8110004,8110005,264773);
INSERT INTO `creature_template_difficulty` (`Entry`,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,`LootID`,`PickPocketLootID`,`SkinLootID`,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild`) VALUES
(8110000,0,0,0,0,0,3.0,1.0,1.0,1.0,0,0,0,0,0,0,0,200,550,0,0,0,0,0,0,0,0,0),
(8110001,0,0,0,0,0,2.0,1.0,1.0,1.0,0,0,0,0,0,0,0,200,550,0,0,0,0,0,0,0,0,0),
(8110002,0,0,0,0,0,2.5,5.0,1.0,1.0,0,0,0,0,0,0,0,200,550,0,0,0,0,0,0,0,0,0),
(8110003,0,0,0,0,0,2.5,5.0,1.0,1.0,0,0,0,0,0,0,0,200,550,0,0,0,0,0,0,0,0,0),
(8110004,0,0,0,0,0,3.5,1.0,1.0,1.0,0,0,0,0,0,0,0,200,550,0,0,0,0,0,0,0,0,0),
(8110005,0,0,0,0,0,2.0,1.0,1.0,1.0,0,0,0,0,0,0,0,200,550,0,0,0,0,0,0,0,0,0),
(264773,0,0,0,0,0,4.0,1.0,1.0,1.5,348274,0,0,0,264773,0,0,800,1800,0,0,0,0,0,0,0,0,0);
DELETE FROM `creature_template_model` WHERE `CreatureID` IN (8110000,8110001,8110002,8110003,8110004,8110005,264773);
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES
(8110000,0,1052,1.0,1.0,0),
(8110001,0,10704,0.9,1.0,0),
(8110002,0,536,1.0,1.0,0),
(8110003,0,7830,1.0,1.0,0),
(8110004,0,11544,1.1,1.0,0),
(8110005,0,782,1.1,1.0,0),
(264773,0,14424,0.9,1.0,0);
DELETE FROM `creature_classic_level` WHERE `entry` IN (8110000,8110001,8110002,8110003,8110004,8110005,264773);
INSERT INTO `creature_classic_level` (`entry`,`level_min`,`level_max`) VALUES
(8110000,42,42),
(8110001,41,41),
(8110002,43,43),
(8110003,42,42),
(8110004,44,44),
(8110005,43,43),
(264773,44,44);

-- boss scripts, gold (INFERRED) and loot ids
UPDATE `creature_template` SET `AIName` = '', `ScriptName` = 'classic_boss_kds_grox' WHERE `entry` = 258966;
UPDATE `creature_template` SET `AIName` = '', `ScriptName` = 'classic_boss_kds_zarmok' WHERE `entry` = 258972;
UPDATE `creature_template` SET `AIName` = '', `ScriptName` = 'classic_boss_kds_ashka' WHERE `entry` = 259281;
UPDATE `creature_template` SET `AIName` = '', `ScriptName` = 'classic_boss_kds_nargurk' WHERE `entry` = 258964;
UPDATE `creature_template` SET `AIName` = '', `ScriptName` = 'classic_boss_kds_mugg' WHERE `entry` = 258968;
UPDATE `creature_template_difficulty` SET `GoldMin` = 2000, `GoldMax` = 4500, `LootID` = `Entry` WHERE `DifficultyID` = 0 AND `Entry` IN (258966,258972,259281,258964,258968);
DELETE FROM `creature_loot_template` WHERE `Entry` IN (258966,258972,259281,258964,258968,264773);
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(258966,0,274995,33.3,0,1,1,1,1,'Slavecrusher Grox: boss item (one each kill, chance INFERRED; list from the guides)'),
(258966,0,274996,33.3,0,1,1,1,1,'Slavecrusher Grox: boss item (one each kill, chance INFERRED; list from the guides)'),
(258966,0,274997,33.4,0,1,1,1,1,'Slavecrusher Grox: boss item (one each kill, chance INFERRED; list from the guides)'),
(258972,0,274998,50.0,0,1,1,1,1,'Twilight Commander Zarmok: boss item (one each kill, chance INFERRED; list from the guides)'),
(258972,0,275000,50.0,0,1,1,1,1,'Twilight Commander Zarmok: boss item (one each kill, chance INFERRED; list from the guides)'),
(259281,0,275001,50.0,0,1,1,1,1,'War Handler Ashka: boss item (one each kill, chance INFERRED; list from the guides)'),
(259281,0,275002,50.0,0,1,1,1,1,'War Handler Ashka: boss item (one each kill, chance INFERRED; list from the guides)'),
(258964,0,275004,50.0,0,1,1,1,1,'Nar''gurk: boss item (one each kill, chance INFERRED; list from the guides)'),
(258964,0,275005,50.0,0,1,1,1,1,'Nar''gurk: boss item (one each kill, chance INFERRED; list from the guides)'),
(258968,0,275007,50.0,0,1,1,1,1,'Overlord Mugg: boss item (one each kill, chance INFERRED; list from the guides)'),
(258968,0,275009,50.0,0,1,1,1,1,'Overlord Mugg: boss item (one each kill, chance INFERRED; list from the guides)'),
(264773,0,286429,50.0,0,1,1,1,1,'Bossman Toof: boss item (one each kill, chance INFERRED; list from the guides)'),
(264773,0,286430,50.0,0,1,1,1,1,'Bossman Toof: boss item (one each kill, chance INFERRED; list from the guides)'),
(258964,0,267104,100,1,1,0,1,1,'Nargurk: Tome of Twilight (quest item, guides)'),
(258968,0,267086,100,1,1,0,1,1,'Overlord Mugg: Eye of Mugg (quest item, guides)');

-- placeholder trash abilities (spells from the 70862 trash kit; peons call for help, guides; timers INFERRED)
DELETE FROM `smart_scripts` WHERE `source_type` = 0 AND `entryorguid` IN (8110000,8110001,8110002,8110003,8110004,8110005);
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`Difficulties`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`event_param_string`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`action_param7`,`action_param_string`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_param4`,`target_param_string`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(8110000,0,0,0,'',9,0,100,0,8,30,12000,16000,0,'',11,1283364,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Krol''dok Brute - Net on victim at range (INFERRED timer)'),
(8110000,0,1,0,'',0,0,100,0,4000,7000,8000,11000,0,'',11,1283413,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Krol''dok Brute - Cleave on victim (INFERRED timer)'),
(8110001,0,0,0,'',4,0,100,0,0,0,0,0,0,'',39,30,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Krol''dok Peon - call for help on aggro'),
(8110002,0,0,0,'',14,0,100,0,1500,40,8000,12000,0,'',11,1283680,0,0,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'Krol''dok Shaman - Healing Wave on an ally missing 1500 health (INFERRED timer)'),
(8110002,0,1,0,'',0,0,100,0,1000,2000,4000,6000,0,'',11,1283421,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Krol''dok Shaman - Lightning Bolt on victim (INFERRED timer)'),
(8110003,0,0,0,'',0,0,100,0,0,1000,3500,5000,0,'',11,1283362,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Twilight Cultist - Shadow Bolt on victim (INFERRED timer)'),
(8110003,0,1,0,'',0,0,100,0,4000,6000,20000,25000,0,'',11,1283361,0,0,0,0,0,0,'',6,0,0,0,0,'',0,0,0,0,'Twilight Cultist - Curse of Weakness on a random player (INFERRED timer)'),
(8110004,0,0,0,'',0,0,100,0,4000,7000,9000,12000,0,'',11,1283414,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Krol''dok Centurion - Mortal Strike on victim (cuts healing) (INFERRED timer)'),
(8110005,0,0,0,'',0,0,100,0,3000,5000,9000,12000,0,'',11,1283327,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Krol''dok War Wolf - Fatal Bite on victim (INFERRED timer)');

-- spawns (spots INFERRED)
DELETE FROM `creature` WHERE `guid` BETWEEN 48110000 AND 48110199;
INSERT INTO `creature` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`modelid`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`currentwaypoint`,`curHealthPct`,`MovementType`,`npcflag`,`unit_flags`,`unit_flags2`,`unit_flags3`,`ScriptName`,`StringId`,`VerifiedBuild`) VALUES
(48110000,258966,3039,0,0,'1,201',0,0,0,-1,0,0,-7455.63,-3769.81,146.42,1.452,86400,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* Slavecrusher Grox */,
(48110001,258972,3039,0,0,'1,201',0,0,0,-1,0,0,-7370.91,-3985.64,166.75,1.522,86400,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* Twilight Commander Zarmok */,
(48110002,259281,3039,0,0,'1,201',0,0,0,-1,0,0,-7544.86,-4177.56,157.15,0.163,86400,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* War Handler Ashka */,
(48110003,258964,3039,0,0,'1,201',0,0,0,-1,0,0,-7367.19,-4323.94,168.59,3.214,86400,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* Nar'gurk */,
(48110004,264773,3039,0,0,'1,201',0,0,0,-1,0,0,-7257.15,-4154.9,228.5,4.871,86400,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* Bossman Toof */,
(48110005,258968,3039,0,0,'1,201',0,0,0,-1,0,0,-7328.87,-4008.9,269.09,5.293,86400,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* Overlord Mugg */,
(48110006,8110005,3039,0,0,'1,201',0,0,0,-1,0,0,-7540.73,-4177.53,157.4,0.163,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* Ashka's wolves */,
(48110007,8110005,3039,0,0,'1,201',0,0,0,-1,0,0,-7548.91,-4178.22,156.65,0.163,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* Ashka's wolves */,
(48110008,8110002,3039,0,0,'1,201',0,0,0,-1,0,0,-7252.43,-4154.07,228.03,4.871,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* Toof's shamans */,
(48110009,8110002,3039,0,0,'1,201',0,0,0,-1,0,0,-7263.09,-4154.1,232.26,4.871,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* Toof's shamans */,
(48110010,8110001,3039,0,0,'1,201',0,0,0,-1,0,0,-7612.2,-3722.98,158.42,2.766,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 60 yd in */,
(48110011,8110001,3039,0,0,'1,201',0,0,0,-1,0,0,-7616.23,-3719.78,157.65,2.766,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 60 yd in */,
(48110012,8110000,3039,0,0,'1,201',0,0,0,-1,0,0,-7616.63,-3724.85,158.38,2.766,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 60 yd in */,
(48110013,8110000,3039,0,0,'1,201',0,0,0,-1,0,0,-7570.72,-3720.33,154.71,3.02,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 108 yd in */,
(48110014,8110003,3039,0,0,'1,201',0,0,0,-1,0,0,-7576.31,-3721.75,155.57,3.02,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 108 yd in */,
(48110015,8110001,3039,0,0,'1,201',0,0,0,-1,0,0,-7532.2,-3736.42,149.15,2.127,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 156 yd in */,
(48110016,8110001,3039,0,0,'1,201',0,0,0,-1,0,0,-7537.96,-3735.43,149.87,2.127,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 156 yd in */,
(48110017,8110000,3039,0,0,'1,201',0,0,0,-1,0,0,-7496.3,-3755.39,146.42,2.881,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 204 yd in */,
(48110018,8110000,3039,0,0,'1,201',0,0,0,-1,0,0,-7500.73,-3750.62,145.48,2.881,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 204 yd in */,
(48110019,8110003,3039,0,0,'1,201',0,0,0,-1,0,0,-7502.99,-3756.8,148.57,2.881,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 204 yd in */,
(48110020,8110003,3039,0,0,'1,201',0,0,0,-1,0,0,-7430.59,-3792.09,151.55,3.211,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 300 yd in */,
(48110021,8110003,3039,0,0,'1,201',0,0,0,-1,0,0,-7435.87,-3789.39,150.65,3.211,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 300 yd in */,
(48110022,8110000,3039,0,0,'1,201',0,0,0,-1,0,0,-7434.95,-3794.66,150.6,3.211,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 300 yd in */,
(48110023,8110001,3039,0,0,'1,201',0,0,0,-1,0,0,-7394.64,-3804.09,158.75,1.863,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 348 yd in */,
(48110024,8110000,3039,0,0,'1,201',0,0,0,-1,0,0,-7399.57,-3804.46,157.23,1.863,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 348 yd in */,
(48110025,8110001,3039,0,0,'1,201',0,0,0,-1,0,0,-7378.9,-3846.08,163.84,2.722,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 396 yd in */,
(48110026,8110001,3039,0,0,'1,201',0,0,0,-1,0,0,-7385.28,-3842.2,163.66,2.722,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 396 yd in */,
(48110027,8110000,3039,0,0,'1,201',0,0,0,-1,0,0,-7384.1,-3847.42,165.42,2.722,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 396 yd in */,
(48110028,8110000,3039,0,0,'1,201',0,0,0,-1,0,0,-7365.33,-3881.29,167.96,1.524,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 444 yd in */,
(48110029,8110003,3039,0,0,'1,201',0,0,0,-1,0,0,-7372.06,-3883.36,168.07,1.524,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 444 yd in */,
(48110030,8110001,3039,0,0,'1,201',0,0,0,-1,0,0,-7367.57,-3922.01,166.86,1.521,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 492 yd in */,
(48110031,8110001,3039,0,0,'1,201',0,0,0,-1,0,0,-7375.32,-3923.45,166.35,1.521,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 492 yd in */,
(48110032,8110000,3039,0,0,'1,201',0,0,0,-1,0,0,-7421.75,-4023.99,161.42,6.217,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 636 yd in */,
(48110033,8110000,3039,0,0,'1,201',0,0,0,-1,0,0,-7426.81,-4020.12,161.38,6.217,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 636 yd in */,
(48110034,8110003,3039,0,0,'1,201',0,0,0,-1,0,0,-7425.52,-4027.02,161.06,6.217,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 636 yd in */,
(48110035,8110003,3039,0,0,'1,201',0,0,0,-1,0,0,-7452.94,-4042.21,156.05,0.401,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 684 yd in */,
(48110036,8110003,3039,0,0,'1,201',0,0,0,-1,0,0,-7459.3,-4039.31,155.25,0.401,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 684 yd in */,
(48110037,8110000,3039,0,0,'1,201',0,0,0,-1,0,0,-7458.93,-4043.76,154.94,0.401,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 684 yd in */,
(48110038,8110001,3039,0,0,'1,201',0,0,0,-1,0,0,-7481.37,-4073.89,152.47,1.18,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 732 yd in */,
(48110039,8110000,3039,0,0,'1,201',0,0,0,-1,0,0,-7486.4,-4074.46,151.63,1.18,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 732 yd in */,
(48110040,8110001,3039,0,0,'1,201',0,0,0,-1,0,0,-7516.84,-4088.58,151.89,1.571,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 780 yd in */,
(48110041,8110001,3039,0,0,'1,201',0,0,0,-1,0,0,-7521.04,-4087.52,151.73,1.571,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 780 yd in */,
(48110042,8110000,3039,0,0,'1,201',0,0,0,-1,0,0,-7522.55,-4093.13,151.67,1.571,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 780 yd in */,
(48110043,8110000,3039,0,0,'1,201',0,0,0,-1,0,0,-7529.09,-4130.8,153.01,5.926,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 828 yd in */,
(48110044,8110003,3039,0,0,'1,201',0,0,0,-1,0,0,-7535.36,-4129.39,153.86,5.926,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 828 yd in */,
(48110045,8110001,3039,0,0,'1,201',0,0,0,-1,0,0,-7506.67,-4221.92,158.72,1.815,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 972 yd in */,
(48110046,8110001,3039,0,0,'1,201',0,0,0,-1,0,0,-7506.67,-4221.92,158.72,1.815,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 972 yd in */,
(48110047,8110000,3039,0,0,'1,201',0,0,0,-1,0,0,-7482.3,-4253.02,160.1,2.605,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 1020 yd in */,
(48110048,8110000,3039,0,0,'1,201',0,0,0,-1,0,0,-7485.48,-4253.5,160.78,2.605,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 1020 yd in */,
(48110049,8110003,3039,0,0,'1,201',0,0,0,-1,0,0,-7488.92,-4252.07,160.91,2.605,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 1020 yd in */,
(48110050,8110004,3039,0,0,'1,201',0,0,0,-1,0,0,-7484.98,-4256.05,158.4,2.605,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 1020 yd in */,
(48110051,8110003,3039,0,0,'1,201',0,0,0,-1,0,0,-7470.1,-4290.72,156.61,2.248,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 1068 yd in */,
(48110052,8110003,3039,0,0,'1,201',0,0,0,-1,0,0,-7473.23,-4287.89,157.63,2.248,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 1068 yd in */,
(48110053,8110000,3039,0,0,'1,201',0,0,0,-1,0,0,-7472.78,-4290.69,156.67,2.248,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 1068 yd in */,
(48110054,8110001,3039,0,0,'1,201',0,0,0,-1,0,0,-7434.56,-4314.49,158.84,2.195,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 1116 yd in */,
(48110055,8110000,3039,0,0,'1,201',0,0,0,-1,0,0,-7440.73,-4313.96,158.17,2.195,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 1116 yd in */,
(48110056,8110001,3039,0,0,'1,201',0,0,0,-1,0,0,-7393.5,-4323.08,166.38,2.702,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 1164 yd in */,
(48110057,8110001,3039,0,0,'1,201',0,0,0,-1,0,0,-7396.21,-4320.57,166.36,2.702,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 1164 yd in */,
(48110058,8110000,3039,0,0,'1,201',0,0,0,-1,0,0,-7398.56,-4322.95,165.3,2.702,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 1164 yd in */,
(48110059,8110004,3039,0,0,'1,201',0,0,0,-1,0,0,-7396.82,-4326.33,164.92,2.702,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 1164 yd in */,
(48110060,8110000,3039,0,0,'1,201',0,0,0,-1,0,0,-7316.36,-4316.28,175.04,4.223,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 1260 yd in */,
(48110061,8110003,3039,0,0,'1,201',0,0,0,-1,0,0,-7322.73,-4317.44,174.95,4.223,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 1260 yd in */,
(48110062,8110001,3039,0,0,'1,201',0,0,0,-1,0,0,-7276.01,-4299.16,182.25,3.864,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 1308 yd in */,
(48110063,8110001,3039,0,0,'1,201',0,0,0,-1,0,0,-7281.97,-4299.63,181.23,3.864,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 1308 yd in */,
(48110064,8110000,3039,0,0,'1,201',0,0,0,-1,0,0,-7256.35,-4268.16,188.87,4.462,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 1356 yd in */,
(48110065,8110000,3039,0,0,'1,201',0,0,0,-1,0,0,-7258.7,-4263.58,191.53,4.462,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 1356 yd in */,
(48110066,8110003,3039,0,0,'1,201',0,0,0,-1,0,0,-7263.62,-4268.41,189.11,4.462,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 1356 yd in */,
(48110067,8110004,3039,0,0,'1,201',0,0,0,-1,0,0,-7259.67,-4271.51,188.2,4.462,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 1356 yd in */,
(48110068,8110003,3039,0,0,'1,201',0,0,0,-1,0,0,-7253.69,-4224.12,200.58,5.018,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 1404 yd in */,
(48110069,8110003,3039,0,0,'1,201',0,0,0,-1,0,0,-7259.37,-4222.83,203.92,5.018,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 1404 yd in */,
(48110070,8110000,3039,0,0,'1,201',0,0,0,-1,0,0,-7260.04,-4227.09,204.8,5.018,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 1404 yd in */,
(48110071,8110001,3039,0,0,'1,201',0,0,0,-1,0,0,-7240.59,-4101.38,247.45,4.662,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 1548 yd in */,
(48110072,8110000,3039,0,0,'1,201',0,0,0,-1,0,0,-7247.1,-4101.85,248.76,4.662,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 1548 yd in */,
(48110073,8110001,3039,0,0,'1,201',0,0,0,-1,0,0,-7251.54,-4061.54,257.22,0.167,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 1596 yd in */,
(48110074,8110001,3039,0,0,'1,201',0,0,0,-1,0,0,-7253.25,-4058.84,257.51,0.167,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 1596 yd in */,
(48110075,8110000,3039,0,0,'1,201',0,0,0,-1,0,0,-7257.06,-4062.18,256.7,0.167,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 1596 yd in */,
(48110076,8110004,3039,0,0,'1,201',0,0,0,-1,0,0,-7254.96,-4065.08,256.07,0.167,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 1596 yd in */,
(48110077,8110000,3039,0,0,'1,201',0,0,0,-1,0,0,-7282.43,-4043.34,261.0,6.14,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 1644 yd in */,
(48110078,8110003,3039,0,0,'1,201',0,0,0,-1,0,0,-7288.82,-4044.11,261.45,6.14,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, 1644 yd in */;

-- the way in: the cave at Riverglades 37.5, 37.2 (map 0). Map 3039 is built on the same terrain, so the landing is the same cave.
DELETE FROM `world_safe_locs` WHERE `ID` IN (1104122,1104123);
INSERT INTO `world_safe_locs` (`ID`,`MapID`,`LocX`,`LocY`,`LocZ`,`Facing`,`Comment`) VALUES
(1104122,3039,-7659.97,-3713.1,157.51,0.315,'Krol''dok Stronghold (dungeon) - entrance inside, the cave (INFERRED)'),
(1104123,0,-7659.97,-3713.1,157.51,3.457,'Krol''dok Stronghold (dungeon) - exit outside, the cave in Riverglades (INFERRED)');
DELETE FROM `areatrigger_template` WHERE `IsCustom` = 1 AND `Id` IN (322,323);
INSERT INTO `areatrigger_template` (`Id`,`IsCustom`,`Flags`,`ActionSetId`,`ActionSetFlags`,`VerifiedBuild`) VALUES
(322,1,0,0,0,0),
(323,1,0,0,0,0);
DELETE FROM `areatrigger_template_actions` WHERE `IsCustom` = 1 AND `AreaTriggerId` IN (322,323);
INSERT INTO `areatrigger_template_actions` (`AreaTriggerId`,`IsCustom`,`ActionType`,`ActionParam`,`TargetType`) VALUES
(322,1,2,1104122,5),
(323,1,2,1104123,5);
DELETE FROM `areatrigger_create_properties` WHERE `IsCustom` = 1 AND `Id` IN (322,323);
INSERT INTO `areatrigger_create_properties` (`Id`,`IsCustom`,`AreaTriggerId`,`IsAreatriggerCustom`,`Flags`,`MoveCurveId`,`ScaleCurveId`,`MorphCurveId`,`FacingCurveId`,`AnimId`,`AnimKitId`,`DecalPropertiesId`,`SpellForVisuals`,`PositionalSoundKitId`,`TimeToTargetScale`,`Speed`,`SpeedIsTime`,`Shape`,`ShapeData0`,`ShapeData1`,`ShapeData2`,`ShapeData3`,`ShapeData4`,`ShapeData5`,`ShapeData6`,`ShapeData7`,`Roll`,`Pitch`,`Yaw`,`TargetRoll`,`TargetPitch`,`TargetYaw`,`ScriptName`,`VerifiedBuild`) VALUES
(322,1,322,1,0,0,0,0,0,-1,0,0,NULL,0,0,0,0,0,4,4,0,0,0,0,0,0,0,0,0,NULL,NULL,NULL,'',0),
(323,1,323,1,0,0,0,0,0,-1,0,0,NULL,0,0,0,0,0,4,4,0,0,0,0,0,0,0,0,0,NULL,NULL,NULL,'',0);
DELETE FROM `areatrigger` WHERE `SpawnId` IN (32,33);
INSERT INTO `areatrigger` (`SpawnId`,`AreaTriggerCreatePropertiesId`,`IsCustom`,`MapId`,`SpawnDifficulties`,`PosX`,`PosY`,`PosZ`,`Orientation`,`PhaseUseFlags`,`PhaseId`,`PhaseGroup`,`ScriptName`,`Comment`,`VerifiedBuild`) VALUES
(32,322,1,0,'0',-7669.7,-3710.4,157.5,0,1,0,0,'','Krol''dok Stronghold - dungeon entrance (the cave, Riverglades; INFERRED spot)',0),
(33,323,1,3039,'1,201',-7669.7,-3710.4,157.5,0,1,0,0,'','Krol''dok Stronghold (dungeon) - exit (the cave; INFERRED spot)',0);

-- ghosts release to Turner's Logging Camp (nearest Riverglades graveyard)
DELETE FROM `graveyard_zone` WHERE `GhostZone` = 16842;
INSERT INTO `graveyard_zone` (`ID`,`GhostZone`,`Comment`) VALUES
(1101203,16842,'Krol''dok Stronghold (dungeon) - Riverglades, Turner''s Logging Camp');

DELETE FROM `game_tele` WHERE `name` IN ('KroldokStronghold','KroldokStrongholdEntrance');
INSERT INTO `game_tele` (`id`,`position_x`,`position_y`,`position_z`,`orientation`,`map`,`name`) VALUES
(2352,-7659.97,-3713.1,157.51,0.315,3039,'KroldokStronghold'),
(2353,-7659.97,-3713.1,157.51,3.457,0,'KroldokStrongholdEntrance');
