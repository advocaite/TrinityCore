-- City of Dalaran (Forever dungeon, map 2959, levels 28-33): make the dungeon playable.
-- The spawns, boss templates and boss loot were already in the world DB (2026_11_05_99), but the map had no instance_template
-- row, the bosses' ScriptNames had no code, the last boss had no template, the way in did nothing and the portal to the last
-- boss cast a spell the client does not have. The fights are in classic_instance_city_of_dalaran.cpp.
-- No sniff exists for this dungeon: everything marked INFERRED is a guess (see the .cpp header for the sources).

-- the instance
DELETE FROM `instance_template` WHERE `map` = 2959;
INSERT INTO `instance_template` (`map`,`parent`,`script`) VALUES
(2959,0,'classic_instance_city_of_dalaran');

-- boss scripts (the old names boss_* had no code)
UPDATE `creature_template` SET `ScriptName` = 'classic_boss_cod_atrexis' WHERE `entry` = 247126;
UPDATE `creature_template` SET `ScriptName` = 'classic_boss_cod_lyn' WHERE `entry` = 247032;
UPDATE `creature_template` SET `ScriptName` = 'classic_boss_cod_arcane_anomaly' WHERE `entry` = 245999;
UPDATE `creature_template` SET `ScriptName` = 'classic_boss_cod_arcanic_enigma' WHERE `entry` = 246016;
UPDATE `creature_template` SET `ScriptName` = 'classic_boss_cod_unstable_sentinel' WHERE `entry` = 246017;
UPDATE `creature_template` SET `ScriptName` = 'classic_boss_cod_mana_devourer' WHERE `entry` = 246008;
UPDATE `creature_template` SET `ScriptName` = 'classic_boss_cod_fel_ancient' WHERE `entry` = 246003;

-- the way in (Sewer Grate outside the city, map 0) and the portals to and from the Purple Parlor
UPDATE `gameobject_template` SET `ScriptName` = 'classic_go_cod_sewer_grate' WHERE `entry` = 656283;
UPDATE `gameobject_template` SET `ScriptName` = 'classic_go_cod_portal_to_parlor' WHERE `entry` = 656243;
UPDATE `gameobject_template` SET `ScriptName` = 'classic_go_cod_portal_from_parlor' WHERE `entry` = 656320;

-- Shade of the Archmage 246020 (id, name and loot from the public guides; stats INFERRED from the other bosses of the dungeon:
-- level 33, a little more health than Unstable Sentinel). Model: the Kirin Tor Wizard model 3585, scaled up (INFERRED: the live
-- model is not in our data).
DELETE FROM `creature_template` WHERE `entry` = 246020;
INSERT INTO `creature_template` (`entry`,`KillCredit1`,`KillCredit2`,`name`,`femaleName`,`subname`,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,`npcflag`,`speed_walk`,`speed_run`,`scale`,`Classification`,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,`family`,`trainer_class`,`type`,`VehicleId`,`AIName`,`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,`ScriptName`,`StringId`,`VerifiedBuild`) VALUES
(246020,0,0,'Shade of the Archmage',NULL,NULL,NULL,NULL,0,0,3561,0,1.0,1.14286,1.0,1,0,2000,2000,1.0,1.0,8,64,2048,0,0,0,6,0,'',0,1.0,0,1694,0,0,1,0,0,'classic_boss_cod_shade_of_the_archmage',NULL,0);

DELETE FROM `creature_template_difficulty` WHERE `Entry` = 246020;
INSERT INTO `creature_template_difficulty` (`Entry`,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,`LootID`,`PickPocketLootID`,`SkinLootID`,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild`) VALUES
(246020,0,0,0,0,0,14.0,10.0,1.0,3.5,0,2097256,128,0,246020,0,0,200,600,0,0,0,0,0,0,0,0,0);

DELETE FROM `creature_template_model` WHERE `CreatureID` = 246020;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES
(246020,0,3585,1.3,1.0,0);

DELETE FROM `creature_classic_level` WHERE `entry` = 246020;
INSERT INTO `creature_classic_level` (`entry`,`level_min`,`level_max`) VALUES
(246020,33,33);

-- Shade loot (guides list the four items; one of the three gear pieces each kill like the other bosses, rates INFERRED;
-- the blueprint at the Unstable Sentinel's blueprint rate)
DELETE FROM `creature_loot_template` WHERE `Entry` = 246020;
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(246020,0,273049,33.3,0,1,1,1,1,'Shade of the Archmage: Archmagister\'s Faceted Pendant (chance INFERRED)'),
(246020,0,273051,33.3,0,1,1,1,1,'Shade of the Archmage: Violet Sorcerer\'s Mantle (chance INFERRED)'),
(246020,0,273052,33.4,0,1,1,1,1,'Shade of the Archmage: Ponderous Orb (chance INFERRED)'),
(246020,0,273103,7.7,0,1,0,1,1,'Shade of the Archmage: Blueprint: Arcane Salvager (chance INFERRED, Sentinel blueprint rate)'),
(246020,1,9302620,1.5,0,1,0,1,1,'Untranslated mage scroll (INFERRED world drop)');

-- Shade spawn in the Purple Parlor, facing the portal (spot INFERRED: middle of the room's floor, on the navmesh)
DELETE FROM `creature` WHERE `guid` = 24602000;
INSERT INTO `creature` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`modelid`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`currentwaypoint`,`curHealthPct`,`MovementType`,`npcflag`,`unit_flags`,`unit_flags2`,`unit_flags3`,`ScriptName`,`StringId`,`VerifiedBuild`) VALUES
(24602000,246020,2959,0,0,'1,201',0,0,0,-1,0,0,456.0,356.5,295.0,3.14,86400,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0);
