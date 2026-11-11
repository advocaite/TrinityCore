-- WoW Forever: The Drowned City (map 2970, levels 35-40) made playable: instance, way in and out, trash and boss spawns,
-- trash abilities, boss loot and the quest drops. The fights are in classic_instance_the_drowned_city.cpp.
-- No sniff of this dungeon exists. What is real data and what is a guess:
--  * real (70862 client files): the map, its navmesh (every spawn below sits on the navmesh piece the whole dungeon is
--    on), the encounter list, the spells; boss loot item ids exist in our hotfixes (live pushes and players' caches).
--  * real (players' caches, live realm 70862): the new templates below (name, type, class, models, health and power
--    multipliers, difficulty id) and which mobs carry the quest items (Voodoo-Infused Artifact 270333, I'lalai Fetish 270343).
--  * from the public dungeon guides: the entrance (Sunken Hollow, Stranglethorn 21.4, 27.8), the boss order, which trash
--    stands where, the boss loot lists.
--  * INFERRED (our guesses, every row says so): every spawn spot and pack, levels, drop chances, gold, trash ability
--    timers, the landing spot, the exit spot. Replace them with sniff data when someone records a run.
-- Spawn guids 48100000-48100299. Safe to run again.

-- the instance
DELETE FROM `instance_template` WHERE `map` = 2970;
INSERT INTO `instance_template` (`map`,`parent`,`script`) VALUES
(2970,0,'classic_instance_the_drowned_city');

-- new templates from players' caches (levels and faction 16 INFERRED, like 2026_11_10_81)
DELETE FROM `creature_template` WHERE `entry` IN (258792,268515,259979,259981,259987,260353,263742,264177,250187);
INSERT INTO `creature_template` (`entry`,`KillCredit1`,`KillCredit2`,`name`,`femaleName`,`subname`,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,`npcflag`,`speed_walk`,`speed_run`,`scale`,`Classification`,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,`family`,`trainer_class`,`type`,`VehicleId`,`AIName`,`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,`ScriptName`,`StringId`,`VerifiedBuild`) VALUES
(258792,0,0,'Risen Sentry',NULL,NULL,NULL,NULL,0,0,16,0,1.0,1.14286,1.0,1,0,2000,2000,1.0,1.0,1,0,2048,0,0,0,6,0,'',0,1.0,0,0,0,0,1,0,0,'',NULL,0),
(268515,0,0,'Risen Sentry',NULL,NULL,NULL,NULL,0,0,16,0,1.0,1.14286,1.0,1,0,2000,2000,1.0,1.0,1,0,2048,0,0,0,6,0,'',0,1.0,0,0,0,0,1,0,0,'',NULL,0),
(259979,0,0,'Saltseer Murderer',NULL,NULL,NULL,NULL,0,0,16,0,1.0,1.14286,1.0,1,0,2000,2000,1.0,1.0,1,0,2048,0,0,0,7,0,'',0,1.0,0,0,0,0,1,0,0,'',NULL,0),
(259981,0,0,'Saltseer Cultist',NULL,NULL,NULL,NULL,0,0,16,0,1.0,1.14286,1.0,1,0,2000,2000,1.0,1.0,2,0,2048,0,0,0,7,0,'',0,1.0,0,0,0,0,1,0,0,'',NULL,0),
(259987,0,0,'Saltseer Manhunter',NULL,NULL,NULL,NULL,0,0,16,0,1.0,1.14286,1.0,1,0,2000,2000,1.0,1.0,1,0,2048,0,0,0,7,0,'',0,1.0,0,0,0,0,1,0,0,'',NULL,0),
(260353,0,0,'Saltseer Coroner',NULL,NULL,NULL,NULL,0,0,16,0,1.0,1.14286,1.0,1,0,2000,2000,1.0,1.0,2,0,2048,0,0,0,7,0,'',0,1.0,0,0,0,0,1,0,0,'',NULL,0),
(263742,0,0,'Zin''aka',NULL,NULL,NULL,NULL,0,0,16,0,1.0,1.14286,1.0,1,0,2000,2000,1.0,1.0,1,0,2048,0,0,0,1,0,'',0,1.0,0,0,0,0,1,0,0,'',NULL,0),
(264177,0,0,'Gill',NULL,NULL,NULL,NULL,0,0,16,0,1.0,1.14286,1.0,2,0,2000,2000,1.0,1.0,1,0,2048,0,0,0,7,0,'',0,1.0,0,0,0,0,1,0,0,'',NULL,0),
(250187,0,0,'Bladetooth Frenzy',NULL,NULL,NULL,NULL,0,0,16,0,1.0,1.14286,1.0,1,0,2000,2000,1.0,1.0,1,0,2048,0,0,0,1,0,'',0,1.0,0,0,0,0,1,0,0,'',NULL,0);
DELETE FROM `creature_template_difficulty` WHERE `Entry` IN (258792,268515,259979,259981,259987,260353,263742,264177,250187);
INSERT INTO `creature_template_difficulty` (`Entry`,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,`LootID`,`PickPocketLootID`,`SkinLootID`,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild`) VALUES
(258792,0,0,0,0,0,3.0,1.0,1.0,1.0,341929,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0),
(268515,0,0,0,0,0,3.5,1.0,1.0,1.0,352135,0,67108864,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0),
(259979,0,0,0,0,0,3.0,1.0,1.0,1.0,343172,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0),
(259981,0,0,0,0,0,2.5,5.0,1.0,1.0,343174,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0),
(259987,0,0,0,0,0,2.8,1.0,1.0,1.0,343180,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0),
(260353,0,0,0,0,0,3.0,5.0,1.0,1.0,343560,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0),
(263742,0,0,0,0,0,10.0,1.0,1.0,1.0,347059,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0),
(264177,0,0,0,0,0,3.2,1.0,1.0,1.0,347515,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0),
(250187,0,0,0,0,0,2.5,1.0,1.0,1.0,329780,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);
DELETE FROM `creature_template_model` WHERE `CreatureID` IN (258792,268515,259979,259981,259987,260353,263742,264177,250187);
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES
(258792,0,145398,1.0,1.0,0),
(258792,1,140992,1.0,1.0,0),
(268515,0,145398,1.0,1.0,0),
(268515,1,140992,1.0,1.0,0),
(259979,0,142126,1.0,1.0,0),
(259979,1,142127,1.0,1.0,0),
(259981,0,142129,1.0,1.0,0),
(259981,1,142147,1.0,1.0,0),
(259981,2,142148,1.0,1.0,0),
(259981,3,142130,1.0,1.0,0),
(259981,4,142149,1.0,1.0,0),
(259981,5,142150,1.0,1.0,0),
(259987,0,142131,1.0,1.0,0),
(259987,1,142132,1.0,1.0,0),
(260353,0,142401,1.0,1.0,0),
(260353,1,142405,1.0,1.0,0),
(260353,2,142406,1.0,1.0,0),
(260353,3,142407,1.0,1.0,0),
(260353,4,142408,1.0,1.0,0),
(260353,5,142409,1.0,1.0,0),
(263742,0,143835,1.0,1.0,0),
(264177,0,144023,1.0,1.0,0),
(250187,0,15101,0.5,1.0,0),
(250187,1,15234,0.5,1.0,0);
DELETE FROM `creature_classic_level` WHERE `entry` IN (258792,268515,259979,259981,259987,260353,263742,264177,250187);
INSERT INTO `creature_classic_level` (`entry`,`level_min`,`level_max`) VALUES
(258792,37,37),
(268515,37,37),
(259979,36,36),
(259981,36,36),
(259987,36,36),
(260353,37,37),
(263742,40,40),
(264177,38,38),
(250187,36,36);

-- boss scripts
UPDATE `creature_template` SET `AIName` = '', `ScriptName` = 'classic_boss_tdc_zul_alai' WHERE `entry` = 258905;
UPDATE `creature_template` SET `AIName` = '', `ScriptName` = 'classic_boss_tdc_var_taka' WHERE `entry` = 258924;
UPDATE `creature_template` SET `AIName` = '', `ScriptName` = 'classic_boss_tdc_dreadrise' WHERE `entry` = 260042;
UPDATE `creature_template` SET `AIName` = '', `ScriptName` = 'classic_boss_tdc_deathless_marrow' WHERE `entry` = 260202;
UPDATE `creature_template` SET `AIName` = '', `ScriptName` = 'classic_boss_tdc_primeval_elemental' WHERE `entry` = 263886;
UPDATE `creature_template` SET `AIName` = '', `ScriptName` = 'classic_boss_tdc_min_loth' WHERE `entry` = 260243;

-- gold (INFERRED) and loot ids
UPDATE `creature_template_difficulty` SET `GoldMin` = 150, `GoldMax` = 450 WHERE `DifficultyID` = 0 AND `Entry` IN (258794,258797,268516,258799,258804,260096,258818,258897,260205,260213,260214,268476,268477,268478,263196,268517,263229,263899,258792,268515,259979,259981,259987,260353,263742,264177,250187);
UPDATE `creature_template_difficulty` SET `GoldMin` = 1500, `GoldMax` = 3500, `LootID` = `Entry` WHERE `DifficultyID` = 0 AND `Entry` IN (258905,258924,260042,260202,263886,260243);
UPDATE `creature_template_difficulty` SET `LootID` = `Entry` WHERE `DifficultyID` = 0 AND `Entry` IN (258792,268515,263196,268517,259979,259987,260353);
DELETE FROM `creature_loot_template` WHERE `Entry` IN (258905,258924,260042,260202,263886,260243,258792,268515,263196,268517,259979,259987,260353);
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(258905,0,274598,33.3,0,1,1,1,1,'Zul''Alai: boss item (one each kill, chance INFERRED; list from the guides)'),
(258905,0,274599,33.3,0,1,1,1,1,'Zul''Alai: boss item (one each kill, chance INFERRED; list from the guides)'),
(258905,0,274600,33.4,0,1,1,1,1,'Zul''Alai: boss item (one each kill, chance INFERRED; list from the guides)'),
(258924,0,274601,50.0,0,1,1,1,1,'Var''Taka: boss item (one each kill, chance INFERRED; list from the guides)'),
(258924,0,274602,50.0,0,1,1,1,1,'Var''Taka: boss item (one each kill, chance INFERRED; list from the guides)'),
(260042,0,274603,33.3,0,1,1,1,1,'Captain Dreadrise: boss item (one each kill, chance INFERRED; list from the guides)'),
(260042,0,274604,33.3,0,1,1,1,1,'Captain Dreadrise: boss item (one each kill, chance INFERRED; list from the guides)'),
(260042,0,274605,33.4,0,1,1,1,1,'Captain Dreadrise: boss item (one each kill, chance INFERRED; list from the guides)'),
(260202,0,274606,50.0,0,1,1,1,1,'Deathless Marrow: boss item (one each kill, chance INFERRED; list from the guides)'),
(260202,0,274607,50.0,0,1,1,1,1,'Deathless Marrow: boss item (one each kill, chance INFERRED; list from the guides)'),
(263886,0,274611,20.0,0,1,1,1,1,'Primeval Elemental: boss item (one each kill, chance INFERRED; list from the guides)'),
(263886,0,274612,20.0,0,1,1,1,1,'Primeval Elemental: boss item (one each kill, chance INFERRED; list from the guides)'),
(263886,0,274613,20.0,0,1,1,1,1,'Primeval Elemental: boss item (one each kill, chance INFERRED; list from the guides)'),
(263886,0,274614,20.0,0,1,1,1,1,'Primeval Elemental: boss item (one each kill, chance INFERRED; list from the guides)'),
(263886,0,274615,20.0,0,1,1,1,1,'Primeval Elemental: boss item (one each kill, chance INFERRED; list from the guides)'),
(260243,0,274608,33.3,0,1,1,1,1,'Min''loth the Serpent: boss item (one each kill, chance INFERRED; list from the guides)'),
(260243,0,274609,33.3,0,1,1,1,1,'Min''loth the Serpent: boss item (one each kill, chance INFERRED; list from the guides)'),
(260243,0,274610,33.4,0,1,1,1,1,'Min''loth the Serpent: boss item (one each kill, chance INFERRED; list from the guides)'),
(260202,0,276894,100,0,1,0,1,1,'Deathless Marrow: Ancient Gurubashi Idol (guides)'),
(258792,0,270343,35,1,1,0,1,1,'Risen Sentry: I''lalai Fetish (quest drop from the cache, chance INFERRED)'),
(268515,0,270343,35,1,1,0,1,1,'Risen Sentry: I''lalai Fetish (quest drop from the cache, chance INFERRED)'),
(263196,0,270343,35,1,1,0,1,1,'Profane Necromancer: I''lalai Fetish (quest drop from the cache, chance INFERRED)'),
(268517,0,270343,35,1,1,0,1,1,'Profane Necromancer: I''lalai Fetish (quest drop from the cache, chance INFERRED)'),
(259979,0,270333,35,1,1,0,1,1,'Saltseer Murderer: Voodoo-Infused Artifact (quest drop from the cache, chance INFERRED)'),
(259987,0,270333,35,1,1,0,1,1,'Saltseer Manhunter: Voodoo-Infused Artifact (quest drop from the cache, chance INFERRED)'),
(260353,0,270333,35,1,1,0,1,1,'Saltseer Coroner: Voodoo-Infused Artifact (quest drop from the cache, chance INFERRED)');

-- trash abilities (spells from the 70862 trash kit 1283361-1283425 and Conjure Spirit; which mob uses which from the guides and
-- the mob's role, timers INFERRED)
UPDATE `creature_template` SET `AIName` = 'SmartAI', `ScriptName` = '' WHERE `entry` IN (258897,258818,258792,268515,263196,268517,258804,258799,260096,259979,259981,259987,260353,260205,268477,260213,268478,260214,268476,258797,268516,258794,263742,264177);
DELETE FROM `smart_scripts` WHERE `source_type` = 0 AND `entryorguid` IN (258897,258818,258792,268515,263196,268517,258804,258799,260096,259979,259981,259987,260353,260205,268477,260213,268478,260214,268476,258797,268516,258794,263742,264177);
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`Difficulties`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`event_param_string`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`action_param7`,`action_param_string`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_param4`,`target_param_string`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(258897,0,0,0,'',0,0,100,0,3000,6000,8000,12000,0,'',11,1283390,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Makrura Snapper - Sunder Armor on victim (INFERRED timer)'),
(258818,0,0,0,'',0,0,100,0,4000,7000,7000,10000,0,'',11,1283405,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Chasm Crawler - Muscle Tear on victim (INFERRED timer)'),
(258792,0,0,0,'',4,0,100,0,0,0,0,0,0,'',11,1283407,0,0,0,0,0,0,'',5,0,0,0,0,'',0,0,0,0,'Risen Sentry - Shield Charge on aggro (INFERRED timer)'),
(258792,0,1,0,'',0,0,100,0,6000,9000,10000,14000,0,'',11,1283424,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Risen Sentry - Knock Away on victim (INFERRED timer)'),
(268515,0,0,0,'',4,0,100,0,0,0,0,0,0,'',11,1283407,0,0,0,0,0,0,'',5,0,0,0,0,'',0,0,0,0,'Risen Sentry - Shield Charge on aggro (INFERRED timer)'),
(268515,0,1,0,'',0,0,100,0,6000,9000,10000,14000,0,'',11,1283424,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Risen Sentry - Knock Away on victim (INFERRED timer)'),
(263196,0,0,0,'',0,0,100,0,0,1000,3500,5000,0,'',11,1283362,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Profane Necromancer - Shadow Bolt on victim (INFERRED timer)'),
(263196,0,1,0,'',2,0,100,1,0,50,0,0,0,'',11,1289276,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Profane Necromancer - Conjure Spirit at 50% health (INFERRED timer)'),
(268517,0,0,0,'',0,0,100,0,0,1000,3500,5000,0,'',11,1283362,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Profane Necromancer - Shadow Bolt on victim (INFERRED timer)'),
(268517,0,1,0,'',2,0,100,1,0,50,0,0,0,'',11,1289276,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Profane Necromancer - Conjure Spirit at 50% health (INFERRED timer)'),
(258804,0,0,0,'',0,0,100,0,0,1000,3500,5000,0,'',11,1283410,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Brinescale Sorceress - Frostbolt on victim (INFERRED timer)'),
(258804,0,1,0,'',0,0,100,0,10000,15000,20000,25000,0,'',11,1283934,0,0,0,0,0,0,'',6,0,0,0,0,'',0,0,0,0,'Brinescale Sorceress - Fear on a random player (INFERRED timer)'),
(258799,0,0,0,'',9,0,100,0,8,30,12000,16000,0,'',11,1283364,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Brinescale Explorer - Net on victim at range (INFERRED timer)'),
(258799,0,1,0,'',0,0,100,0,5000,8000,8000,11000,0,'',11,1283413,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Brinescale Explorer - Cleave on victim (INFERRED timer)'),
(260096,0,0,0,'',14,0,100,0,1500,40,8000,12000,0,'',11,1283680,0,0,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'Brinescale Priestess - Healing Wave on an ally missing 1500 health (INFERRED timer)'),
(260096,0,1,0,'',0,0,100,0,1000,2000,4000,6000,0,'',11,1283421,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Brinescale Priestess - Lightning Bolt on victim (INFERRED timer)'),
(259979,0,0,0,'',0,0,100,0,3000,6000,9000,13000,0,'',11,1283416,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Saltseer Murderer - Poisonous Stab on victim (INFERRED timer)'),
(259981,0,0,0,'',0,0,100,0,0,1000,3500,5000,0,'',11,1283362,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Saltseer Cultist - Shadow Bolt on victim (INFERRED timer)'),
(259981,0,1,0,'',0,0,100,0,4000,6000,20000,25000,0,'',11,1283361,0,0,0,0,0,0,'',6,0,0,0,0,'',0,0,0,0,'Saltseer Cultist - Curse of Weakness on a random player (INFERRED timer)'),
(259987,0,0,0,'',9,0,100,0,8,30,12000,16000,0,'',11,1283364,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Saltseer Manhunter - Net on victim at range (INFERRED timer)'),
(259987,0,1,0,'',0,0,100,0,1000,2000,4000,6000,0,'',11,1283365,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Saltseer Manhunter - Shadow Shot on victim (INFERRED timer)'),
(260353,0,0,0,'',0,0,100,0,6000,9000,15000,20000,0,'',11,1283409,0,0,0,0,0,0,'',6,0,0,0,0,'',0,0,0,0,'Saltseer Coroner - Hex on a random player (INFERRED timer)'),
(260353,0,1,0,'',0,0,100,0,1000,2000,4000,6000,0,'',11,1283421,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Saltseer Coroner - Lightning Bolt on victim (INFERRED timer)'),
(260205,0,0,0,'',0,0,100,0,5000,8000,9000,13000,0,'',11,1283414,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Deathless Watcher - Mortal Strike on victim (INFERRED timer)'),
(268477,0,0,0,'',0,0,100,0,5000,8000,9000,13000,0,'',11,1283414,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Deathless Watcher - Mortal Strike on victim (INFERRED timer)'),
(260213,0,0,0,'',0,0,100,0,4000,7000,8000,12000,0,'',11,1283390,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Deathless Guardian - Sunder Armor on victim (INFERRED timer)'),
(260213,0,1,0,'',0,0,100,0,8000,12000,18000,22000,0,'',11,1283425,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Deathless Guardian - War Stomp (INFERRED timer)'),
(268478,0,0,0,'',0,0,100,0,4000,7000,8000,12000,0,'',11,1283390,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Deathless Guardian - Sunder Armor on victim (INFERRED timer)'),
(268478,0,1,0,'',0,0,100,0,8000,12000,18000,22000,0,'',11,1283425,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Deathless Guardian - War Stomp (INFERRED timer)'),
(260214,0,0,0,'',0,0,100,0,0,1000,3500,5000,0,'',11,1283420,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Deathless Sorcerer - Frostbolt on victim (INFERRED timer)'),
(268476,0,0,0,'',0,0,100,0,0,1000,3500,5000,0,'',11,1283420,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Deathless Sorcerer - Frostbolt on victim (INFERRED timer)'),
(258797,0,0,0,'',0,0,100,0,4000,6000,8000,11000,0,'',11,1283413,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Champion of the Deep - Cleave on victim (INFERRED timer)'),
(258797,0,1,0,'',0,0,100,0,2000,4000,15000,18000,0,'',11,1283423,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Champion of the Deep - Rend on victim (INFERRED timer)'),
(268516,0,0,0,'',0,0,100,0,4000,6000,8000,11000,0,'',11,1283413,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Champion of the Deep - Cleave on victim (INFERRED timer)'),
(268516,0,1,0,'',0,0,100,0,2000,4000,15000,18000,0,'',11,1283423,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Champion of the Deep - Rend on victim (INFERRED timer)'),
(258794,0,0,0,'',0,0,100,0,6000,9000,15000,20000,0,'',11,1283409,0,0,0,0,0,0,'',6,0,0,0,0,'',0,0,0,0,'Drowned Hexxer - Hex on a random player (INFERRED timer)'),
(258794,0,1,0,'',0,0,100,0,0,1000,3500,5000,0,'',11,1283362,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Drowned Hexxer - Shadow Bolt on victim (INFERRED timer)'),
(263742,0,0,0,'',0,0,100,0,4000,6000,12000,15000,0,'',11,1283423,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Zin''aka - Rend on victim (INFERRED timer)'),
(264177,0,0,0,'',9,0,100,0,8,30,12000,16000,0,'',11,1283364,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Gill - Net on victim at range (INFERRED timer)'),
(264177,0,1,0,'',0,0,100,0,4000,6000,8000,11000,0,'',11,1283413,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Gill - Cleave on victim (INFERRED timer)');

-- spawns (spots INFERRED: bosses where the guides put them, trash packs every 30-40 yd along the walking route between them)
DELETE FROM `creature` WHERE `guid` BETWEEN 48100000 AND 48100299;
INSERT INTO `creature` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`modelid`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`currentwaypoint`,`curHealthPct`,`MovementType`,`npcflag`,`unit_flags`,`unit_flags2`,`unit_flags3`,`ScriptName`,`StringId`,`VerifiedBuild`) VALUES
(48100000,258905,2970,0,0,'1,201',0,0,0,-1,0,0,-13301.3,2464.0,-1.59,4.732,86400,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* Zul'Alai, top of the climb from the cave */,
(48100001,258924,2970,0,0,'1,201',0,0,0,-1,0,0,-13043.3,2761.8,-27.44,2.962,86400,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* Var'Taka, end of the north arm */,
(48100002,260096,2970,0,0,'1,201',0,0,0,-1,0,0,-13044.05,2756.86,-25.81,2.962,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* Brinescale Priestess at Var'Taka's side */,
(48100003,260096,2970,0,0,'1,201',0,0,0,-1,0,0,-13042.26,2766.69,-30.95,2.962,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* Brinescale Priestess at Var'Taka's side */,
(48100004,260042,2970,0,0,'1,201',0,0,0,-1,0,0,-13230.8,2619.3,-17.27,4.09,86400,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* Captain Dreadrise on the ship deck, east plaza */,
(48100005,260202,2970,0,0,'1,201',0,0,0,-1,0,0,-13601.4,2698.9,-47.46,0.396,86400,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* Deathless Marrow, south flooded quarter */,
(48100006,263886,2970,0,0,'1,201',0,0,0,-1,0,0,-13552.1,2719.5,-33.15,0.517,86400,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* Primeval Elemental, hidden until Marrow dies */,
(48100007,260243,2970,0,0,'1,201',0,0,0,-1,0,0,-13320.0,3190.0,2.09,4.842,86400,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* Min'loth the Serpent, west courtyard */,
(48100008,263742,2970,0,0,'1,201',0,0,0,-1,0,0,-13301.3,2784.0,-45.0,0.0,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* Zin'aka, in the central pool */,
(48100009,250187,2970,0,0,'1,201',0,0,0,-1,0,0,-13292.0,2776.0,-46.0,1.0,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* Bladetooth Frenzy, central pool */,
(48100010,250187,2970,0,0,'1,201',0,0,0,-1,0,0,-13310.0,2792.0,-46.0,4.0,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* Bladetooth Frenzy, central pool */,
(48100011,264177,2970,0,0,'1,201',0,0,0,-1,0,0,-13425.76,2800.96,-32.38,4.503,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* Gill (rare elite), west street */,
(48100012,258897,2970,0,0,'1,201',0,0,0,-1,0,0,-13273.91,2246.68,-50.92,3.523,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, cave */,
(48100013,258897,2970,0,0,'1,201',0,0,0,-1,0,0,-13280.45,2245.49,-50.92,3.523,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, cave */,
(48100014,258818,2970,0,0,'1,201',0,0,0,-1,0,0,-13256.51,2275.21,-49.76,4.03,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, cave */,
(48100015,258897,2970,0,0,'1,201',0,0,0,-1,0,0,-13263.35,2275.27,-50.59,4.03,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, cave */,
(48100016,258818,2970,0,0,'1,201',0,0,0,-1,0,0,-13236.83,2297.56,-42.35,5.015,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, cave */,
(48100017,258818,2970,0,0,'1,201',0,0,0,-1,0,0,-13242.99,2301.09,-42.29,5.015,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, cave */,
(48100018,258897,2970,0,0,'1,201',0,0,0,-1,0,0,-13241.56,2296.03,-43.16,5.015,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, cave */,
(48100019,258897,2970,0,0,'1,201',0,0,0,-1,0,0,-13220.12,2328.42,-34.45,3.697,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, cave */,
(48100020,258897,2970,0,0,'1,201',0,0,0,-1,0,0,-13224.07,2328.37,-35.1,3.697,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, cave */,
(48100021,258818,2970,0,0,'1,201',0,0,0,-1,0,0,-13214.3,2359.17,-28.15,4.751,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, cave */,
(48100022,258897,2970,0,0,'1,201',0,0,0,-1,0,0,-13221.56,2360.82,-27.34,4.751,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, cave */,
(48100023,258818,2970,0,0,'1,201',0,0,0,-1,0,0,-13213.57,2390.56,-25.36,3.282,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, cave */,
(48100024,258818,2970,0,0,'1,201',0,0,0,-1,0,0,-13218.75,2392.42,-24.16,3.282,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, cave */,
(48100025,258897,2970,0,0,'1,201',0,0,0,-1,0,0,-13218.02,2386.65,-25.51,3.282,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, cave */,
(48100026,258897,2970,0,0,'1,201',0,0,0,-1,0,0,-13222.55,2413.67,-19.51,4.697,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, cave */,
(48100027,258897,2970,0,0,'1,201',0,0,0,-1,0,0,-13229.96,2414.91,-18.85,4.697,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, cave */,
(48100028,258818,2970,0,0,'1,201',0,0,0,-1,0,0,-13244.7,2423.87,-15.14,6.251,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, cave */,
(48100029,258897,2970,0,0,'1,201',0,0,0,-1,0,0,-13249.98,2423.17,-14.95,6.251,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, cave */,
(48100030,258818,2970,0,0,'1,201',0,0,0,-1,0,0,-13256.85,2449.94,-6.14,4.824,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, cave */,
(48100031,258818,2970,0,0,'1,201',0,0,0,-1,0,0,-13261.15,2453.35,-5.06,4.824,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, cave */,
(48100032,258897,2970,0,0,'1,201',0,0,0,-1,0,0,-13260.73,2448.36,-7.68,4.824,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, cave */,
(48100033,259979,2970,0,0,'1,201',0,0,0,-1,0,0,-13298.01,2538.27,-3.27,4.906,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, east plaza */,
(48100034,259981,2970,0,0,'1,201',0,0,0,-1,0,0,-13304.74,2538.94,-1.05,4.906,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, east plaza */,
(48100035,260353,2970,0,0,'1,201',0,0,0,-1,0,0,-13269.75,2539.86,-5.58,3.211,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, east plaza */,
(48100036,259987,2970,0,0,'1,201',0,0,0,-1,0,0,-13275.16,2540.47,-5.58,3.211,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, east plaza */,
(48100037,259979,2970,0,0,'1,201',0,0,0,-1,0,0,-13243.25,2548.29,-5.54,3.439,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, east plaza */,
(48100038,260353,2970,0,0,'1,201',0,0,0,-1,0,0,-13248.49,2551.21,-5.4,3.439,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, east plaza */,
(48100039,259981,2970,0,0,'1,201',0,0,0,-1,0,0,-13247.7,2544.25,-5.54,3.439,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, east plaza */,
(48100040,259979,2970,0,0,'1,201',0,0,0,-1,0,0,-13238.94,2567.27,-11.04,4.847,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, east plaza */,
(48100041,259981,2970,0,0,'1,201',0,0,0,-1,0,0,-13245.08,2569.57,-11.46,4.847,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, east plaza */,
(48100042,260353,2970,0,0,'1,201',0,0,0,-1,0,0,-13227.02,2594.22,-14.95,5.078,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, east plaza */,
(48100043,259987,2970,0,0,'1,201',0,0,0,-1,0,0,-13234.01,2592.74,-10.37,5.078,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, east plaza */,
(48100044,258792,2970,0,0,'1,201',0,0,0,-1,0,0,-13196.38,2586.64,-14.26,3.672,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, north street */,
(48100045,263196,2970,0,0,'1,201',0,0,0,-1,0,0,-13201.56,2586.66,-14.04,3.672,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, north street */,
(48100046,258792,2970,0,0,'1,201',0,0,0,-1,0,0,-13186.37,2617.37,-24.3,4.72,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, north street */,
(48100047,258792,2970,0,0,'1,201',0,0,0,-1,0,0,-13192.45,2617.52,-24.17,4.72,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, north street */,
(48100048,263196,2970,0,0,'1,201',0,0,0,-1,0,0,-13168.09,2641.11,-30.79,4.764,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, north street */,
(48100049,258792,2970,0,0,'1,201',0,0,0,-1,0,0,-13173.19,2642.73,-31.17,4.764,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, north street */,
(48100050,258792,2970,0,0,'1,201',0,0,0,-1,0,0,-13174.05,2638.2,-31.23,4.764,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, north street */,
(48100051,258792,2970,0,0,'1,201',0,0,0,-1,0,0,-13155.75,2666.33,-33.58,3.385,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, north street */,
(48100052,263196,2970,0,0,'1,201',0,0,0,-1,0,0,-13162.64,2665.15,-33.85,3.385,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, north street */,
(48100053,258792,2970,0,0,'1,201',0,0,0,-1,0,0,-13140.31,2690.01,-34.12,3.411,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, north street */,
(48100054,258792,2970,0,0,'1,201',0,0,0,-1,0,0,-13147.71,2690.1,-34.29,3.411,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, north street */,
(48100055,263196,2970,0,0,'1,201',0,0,0,-1,0,0,-13143.99,2724.91,-34.7,4.772,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, north street */,
(48100056,258792,2970,0,0,'1,201',0,0,0,-1,0,0,-13149.3,2727.63,-34.84,4.772,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, north street */,
(48100057,258792,2970,0,0,'1,201',0,0,0,-1,0,0,-13148.85,2721.32,-34.49,4.772,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, north street */,
(48100058,258792,2970,0,0,'1,201',0,0,0,-1,0,0,-13151.47,2753.37,-33.45,4.347,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, north street */,
(48100059,263196,2970,0,0,'1,201',0,0,0,-1,0,0,-13157.41,2753.8,-33.52,4.347,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, north street */,
(48100060,258799,2970,0,0,'1,201',0,0,0,-1,0,0,-13136.61,2774.13,-30.36,3.49,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, north arm */,
(48100061,258804,2970,0,0,'1,201',0,0,0,-1,0,0,-13143.28,2774.92,-30.55,3.49,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, north arm */,
(48100062,258799,2970,0,0,'1,201',0,0,0,-1,0,0,-13113.97,2768.66,-32.71,3.142,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, north arm */,
(48100063,258799,2970,0,0,'1,201',0,0,0,-1,0,0,-13119.74,2771.77,-32.65,3.142,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, north arm */,
(48100064,258804,2970,0,0,'1,201',0,0,0,-1,0,0,-13119.03,2767.0,-33.05,3.142,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, north arm */,
(48100065,258799,2970,0,0,'1,201',0,0,0,-1,0,0,-13085.33,2768.83,-31.93,2.504,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, north arm */,
(48100066,258804,2970,0,0,'1,201',0,0,0,-1,0,0,-13092.65,2767.66,-32.7,2.504,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, north arm */,
(48100067,260205,2970,0,0,'1,201',0,0,0,-1,0,0,-13290.33,2590.6,-14.67,0.225,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south street */,
(48100068,260213,2970,0,0,'1,201',0,0,0,-1,0,0,-13296.56,2589.66,-14.61,0.225,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south street */,
(48100069,260214,2970,0,0,'1,201',0,0,0,-1,0,0,-13327.37,2583.81,-14.92,0.267,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south street */,
(48100070,260205,2970,0,0,'1,201',0,0,0,-1,0,0,-13333.17,2584.47,-14.92,0.267,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south street */,
(48100071,260205,2970,0,0,'1,201',0,0,0,-1,0,0,-13366.34,2585.8,-15.05,6.156,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south street */,
(48100072,260213,2970,0,0,'1,201',0,0,0,-1,0,0,-13373.77,2585.94,-15.05,6.156,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south street */,
(48100073,260214,2970,0,0,'1,201',0,0,0,-1,0,0,-13404.66,2586.13,-15.02,5.574,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south street */,
(48100074,260205,2970,0,0,'1,201',0,0,0,-1,0,0,-13412.31,2586.61,-15.0,5.574,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south street */,
(48100075,260205,2970,0,0,'1,201',0,0,0,-1,0,0,-13410.37,2623.23,-27.53,4.145,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south street */,
(48100076,260213,2970,0,0,'1,201',0,0,0,-1,0,0,-13417.03,2623.94,-27.83,4.145,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south street */,
(48100077,260214,2970,0,0,'1,201',0,0,0,-1,0,0,-13403.84,2656.44,-31.25,4.451,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south street */,
(48100078,260205,2970,0,0,'1,201',0,0,0,-1,0,0,-13411.47,2657.4,-30.82,4.451,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south street */,
(48100079,260205,2970,0,0,'1,201',0,0,0,-1,0,0,-13404.46,2691.07,-32.64,5.111,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south street */,
(48100080,260213,2970,0,0,'1,201',0,0,0,-1,0,0,-13411.35,2689.91,-31.16,5.111,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south street */,
(48100081,260214,2970,0,0,'1,201',0,0,0,-1,0,0,-13421.6,2721.2,-32.52,4.787,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south street */,
(48100082,260205,2970,0,0,'1,201',0,0,0,-1,0,0,-13427.77,2721.87,-32.52,4.787,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south street */,
(48100083,260205,2970,0,0,'1,201',0,0,0,-1,0,0,-13443.1,2749.05,-32.32,5.047,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south street */,
(48100084,260213,2970,0,0,'1,201',0,0,0,-1,0,0,-13450.63,2749.95,-30.82,5.047,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south street */,
(48100085,260213,2970,0,0,'1,201',0,0,0,-1,0,0,-13449.58,2765.96,-26.34,1.456,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south flooded quarter */,
(48100086,260214,2970,0,0,'1,201',0,0,0,-1,0,0,-13452.63,2770.68,-24.61,1.456,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south flooded quarter */,
(48100087,260205,2970,0,0,'1,201',0,0,0,-1,0,0,-13452.95,2764.51,-25.19,1.456,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south flooded quarter */,
(48100088,263196,2970,0,0,'1,201',0,0,0,-1,0,0,-13441.55,2734.85,-32.36,2.007,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south flooded quarter */,
(48100089,260213,2970,0,0,'1,201',0,0,0,-1,0,0,-13447.89,2735.95,-32.06,2.007,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south flooded quarter */,
(48100090,260205,2970,0,0,'1,201',0,0,0,-1,0,0,-13460.08,2718.41,-32.28,6.139,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south flooded quarter */,
(48100091,260205,2970,0,0,'1,201',0,0,0,-1,0,0,-13464.09,2721.82,-32.18,6.139,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south flooded quarter */,
(48100092,260214,2970,0,0,'1,201',0,0,0,-1,0,0,-13464.38,2716.19,-32.46,6.139,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south flooded quarter */,
(48100093,260213,2970,0,0,'1,201',0,0,0,-1,0,0,-13492.14,2720.47,-32.11,0.14,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south flooded quarter */,
(48100094,260214,2970,0,0,'1,201',0,0,0,-1,0,0,-13496.81,2723.17,-32.07,0.14,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south flooded quarter */,
(48100095,260205,2970,0,0,'1,201',0,0,0,-1,0,0,-13496.39,2716.89,-32.17,0.14,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south flooded quarter */,
(48100096,263196,2970,0,0,'1,201',0,0,0,-1,0,0,-13523.38,2719.77,-31.79,0.236,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south flooded quarter */,
(48100097,260213,2970,0,0,'1,201',0,0,0,-1,0,0,-13529.4,2721.04,-32.25,0.236,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south flooded quarter */,
(48100098,260205,2970,0,0,'1,201',0,0,0,-1,0,0,-13550.39,2719.6,-32.8,5.865,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south flooded quarter */,
(48100099,260205,2970,0,0,'1,201',0,0,0,-1,0,0,-13556.47,2723.19,-36.3,5.865,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south flooded quarter */,
(48100100,260214,2970,0,0,'1,201',0,0,0,-1,0,0,-13555.36,2718.54,-34.64,5.865,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south flooded quarter */,
(48100101,260213,2970,0,0,'1,201',0,0,0,-1,0,0,-13577.86,2720.72,-45.77,1.247,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south flooded quarter */,
(48100102,260214,2970,0,0,'1,201',0,0,0,-1,0,0,-13582.24,2723.39,-46.0,1.247,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south flooded quarter */,
(48100103,260205,2970,0,0,'1,201',0,0,0,-1,0,0,-13583.54,2718.2,-46.07,1.247,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, south flooded quarter */,
(48100104,258797,2970,0,0,'1,201',0,0,0,-1,0,0,-13428.76,2797.96,-31.79,4.503,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, west street */,
(48100105,258794,2970,0,0,'1,201',0,0,0,-1,0,0,-13435.77,2798.73,-30.42,4.503,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, west street */,
(48100106,258797,2970,0,0,'1,201',0,0,0,-1,0,0,-13412.22,2830.12,-31.47,4.911,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, west street */,
(48100107,258804,2970,0,0,'1,201',0,0,0,-1,0,0,-13417.9,2830.3,-32.13,4.911,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, west street */,
(48100108,258797,2970,0,0,'1,201',0,0,0,-1,0,0,-13389.38,2851.83,-31.91,3.587,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, west street */,
(48100109,258794,2970,0,0,'1,201',0,0,0,-1,0,0,-13395.14,2852.12,-31.73,3.587,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, west street */,
(48100110,258797,2970,0,0,'1,201',0,0,0,-1,0,0,-13366.99,2881.72,-32.53,4.127,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, west street */,
(48100111,258804,2970,0,0,'1,201',0,0,0,-1,0,0,-13372.91,2879.67,-32.54,4.127,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, west street */,
(48100112,258797,2970,0,0,'1,201',0,0,0,-1,0,0,-13334.83,2898.82,-31.77,3.608,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, west street */,
(48100113,258794,2970,0,0,'1,201',0,0,0,-1,0,0,-13340.56,2897.38,-31.99,3.608,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, west street */,
(48100114,258797,2970,0,0,'1,201',0,0,0,-1,0,0,-13316.96,2924.93,-31.78,4.68,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, west street */,
(48100115,258804,2970,0,0,'1,201',0,0,0,-1,0,0,-13324.32,2925.48,-31.59,4.68,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, west street */,
(48100116,258797,2970,0,0,'1,201',0,0,0,-1,0,0,-13304.1,2931.49,-31.5,0.169,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, west causeway */,
(48100117,258794,2970,0,0,'1,201',0,0,0,-1,0,0,-13308.14,2935.59,-31.48,0.169,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, west causeway */,
(48100118,258804,2970,0,0,'1,201',0,0,0,-1,0,0,-13309.42,2930.64,-31.75,0.169,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, west causeway */,
(48100119,268516,2970,0,0,'1,201',0,0,0,-1,0,0,-13317.62,2948.15,-23.03,5.051,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, west causeway */,
(48100120,258794,2970,0,0,'1,201',0,0,0,-1,0,0,-13325.17,2946.08,-24.57,5.051,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, west causeway */,
(48100121,258797,2970,0,0,'1,201',0,0,0,-1,0,0,-13316.61,2979.34,-7.16,4.566,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, west causeway */,
(48100122,258797,2970,0,0,'1,201',0,0,0,-1,0,0,-13320.89,2982.16,-7.27,4.566,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, west causeway */,
(48100123,258804,2970,0,0,'1,201',0,0,0,-1,0,0,-13322.29,2977.2,-7.09,4.566,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, west causeway */,
(48100124,258797,2970,0,0,'1,201',0,0,0,-1,0,0,-13317.33,3013.08,-7.52,4.769,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, west causeway */,
(48100125,258794,2970,0,0,'1,201',0,0,0,-1,0,0,-13322.86,3016.6,-7.62,4.769,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, west causeway */,
(48100126,258804,2970,0,0,'1,201',0,0,0,-1,0,0,-13321.2,3009.44,-7.59,4.769,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, west causeway */,
(48100127,268516,2970,0,0,'1,201',0,0,0,-1,0,0,-13313.46,3044.97,-7.45,4.215,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, west causeway */,
(48100128,258794,2970,0,0,'1,201',0,0,0,-1,0,0,-13320.93,3044.98,-7.47,4.215,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, west causeway */,
(48100129,258797,2970,0,0,'1,201',0,0,0,-1,0,0,-13320.12,3078.44,-7.66,4.689,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, west causeway */,
(48100130,258797,2970,0,0,'1,201',0,0,0,-1,0,0,-13325.07,3080.34,-7.64,4.689,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, west causeway */,
(48100131,258804,2970,0,0,'1,201',0,0,0,-1,0,0,-13325.17,3075.38,-7.71,4.689,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, west causeway */,
(48100132,258797,2970,0,0,'1,201',0,0,0,-1,0,0,-13323.3,3108.89,-3.85,4.287,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, west causeway */,
(48100133,258794,2970,0,0,'1,201',0,0,0,-1,0,0,-13329.14,3112.03,-2.06,4.287,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, west causeway */,
(48100134,258804,2970,0,0,'1,201',0,0,0,-1,0,0,-13328.53,3108.49,-4.95,4.287,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, west causeway */,
(48100135,268516,2970,0,0,'1,201',0,0,0,-1,0,0,-13320.79,3134.87,1.29,4.834,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, west causeway */,
(48100136,258794,2970,0,0,'1,201',0,0,0,-1,0,0,-13327.77,3135.84,1.34,4.834,7200,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0) /* trash, west causeway */;

-- the way in: Sunken Hollow, Stranglethorn Vale (map 0). The cave floor spot and the landing spot are on the navmesh (INFERRED spots).
DELETE FROM `world_safe_locs` WHERE `ID` IN (1104120,1104121);
INSERT INTO `world_safe_locs` (`ID`,`MapID`,`LocX`,`LocY`,`LocZ`,`Facing`,`Comment`) VALUES
(1104120,2970,-13296.7,2232.3,-47.5,1.591,'The Drowned City (dungeon) - entrance inside, the cave pool (INFERRED)'),
(1104121,0,-12350.0,854.0,-76.0,5.59,'The Drowned City (dungeon) - exit outside, Sunken Hollow (INFERRED)');
DELETE FROM `areatrigger_template` WHERE `IsCustom` = 1 AND `Id` IN (320,321);
INSERT INTO `areatrigger_template` (`Id`,`IsCustom`,`Flags`,`ActionSetId`,`ActionSetFlags`,`VerifiedBuild`) VALUES
(320,1,0,0,0,0),
(321,1,0,0,0,0);
DELETE FROM `areatrigger_template_actions` WHERE `IsCustom` = 1 AND `AreaTriggerId` IN (320,321);
INSERT INTO `areatrigger_template_actions` (`AreaTriggerId`,`IsCustom`,`ActionType`,`ActionParam`,`TargetType`) VALUES
(320,1,2,1104120,5),
(321,1,2,1104121,5);
DELETE FROM `areatrigger_create_properties` WHERE `IsCustom` = 1 AND `Id` IN (320,321);
INSERT INTO `areatrigger_create_properties` (`Id`,`IsCustom`,`AreaTriggerId`,`IsAreatriggerCustom`,`Flags`,`MoveCurveId`,`ScaleCurveId`,`MorphCurveId`,`FacingCurveId`,`AnimId`,`AnimKitId`,`DecalPropertiesId`,`SpellForVisuals`,`PositionalSoundKitId`,`TimeToTargetScale`,`Speed`,`SpeedIsTime`,`Shape`,`ShapeData0`,`ShapeData1`,`ShapeData2`,`ShapeData3`,`ShapeData4`,`ShapeData5`,`ShapeData6`,`ShapeData7`,`Roll`,`Pitch`,`Yaw`,`TargetRoll`,`TargetPitch`,`TargetYaw`,`ScriptName`,`VerifiedBuild`) VALUES
(320,1,320,1,0,0,0,0,0,-1,0,0,NULL,0,0,0,0,0,5,5,0,0,0,0,0,0,0,0,0,NULL,NULL,NULL,'',0),
(321,1,321,1,0,0,0,0,0,-1,0,0,NULL,0,0,0,0,0,4,4,0,0,0,0,0,0,0,0,0,NULL,NULL,NULL,'',0);
DELETE FROM `areatrigger` WHERE `SpawnId` IN (30,31);
INSERT INTO `areatrigger` (`SpawnId`,`AreaTriggerCreatePropertiesId`,`IsCustom`,`MapId`,`SpawnDifficulties`,`PosX`,`PosY`,`PosZ`,`Orientation`,`PhaseUseFlags`,`PhaseId`,`PhaseGroup`,`ScriptName`,`Comment`,`VerifiedBuild`) VALUES
(30,320,1,0,'0',-12362.0,864.0,-77.5,0,1,0,0,'','The Drowned City - dungeon entrance (Sunken Hollow cave, INFERRED spot)',0),
(31,321,1,2970,'1,201',-13285.0,2215.0,-50.0,0,1,0,0,'','The Drowned City (dungeon) - exit (the cave pool, INFERRED spot)',0);

-- ghosts release to the Savage Coast graveyard (the nearest one to Sunken Hollow)
DELETE FROM `graveyard_zone` WHERE `GhostZone` = 16550;
INSERT INTO `graveyard_zone` (`ID`,`GhostZone`,`Comment`) VALUES
(1460,16550,'The Drowned City (dungeon) - Stranglethorn Vale, Savage Coast');

-- .tele
DELETE FROM `game_tele` WHERE `name` IN ('DrownedCity','DrownedCityEntrance');
INSERT INTO `game_tele` (`id`,`position_x`,`position_y`,`position_z`,`orientation`,`map`,`name`) VALUES
(2350,-13296.7,2232.3,-47.5,1.591,2970,'DrownedCity'),
(2351,-12350.0,854.0,-76.0,5.59,0,'DrownedCityEntrance');
