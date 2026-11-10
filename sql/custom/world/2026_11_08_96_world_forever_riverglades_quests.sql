-- Riverglades (WoW Forever, Classic 1.60): the zone's quests. Our world had the zone's NPCs but none of its quests,
-- no quest objects and no quest drops, so players reaching Powderfuse Port found nothing to do.
-- Built from one player's files sent to us 2026-11-08 (Onlybeams), read as data only:
--  * the player's game cache (Cache\WDB questcache/creaturecache/gameobjectcache, build 70862, tools/forever_cache): quest text, objectives,
--    levels, XP column, rewards, races and flags as Blizzard sends them; NPC and object names and models.
--  * the player's QuestieTrace export (Questie add-on recorder, 3 sessions, 8.5 h on Blizzard's realm): who gives and takes each quest,
--    reward text, progress text, money and XP at turn-in, reward choices, what dropped from whom, and where the player stood.
-- Quests with VerifiedBuild 70862 come from the cache; quests with VerifiedBuild 0 only from the trace (text as the NPC said it,
-- XP column from the XP the player got, objectives matched by name, objective ids 9900000+ are ours).
-- INFERRED, check in game: spots of new NPCs and objects are where the player stood (map point turned into a world spot with a fit
-- from 13 of our own quest givers there, error under 10 yd); heights are taken from our nearest spawns (NPCs 2 yd higher, the server puts them on the ground when they move); drop chances are
-- from what the player looted (min 25%), 35% where only the cache says an NPC carries the item; turn-in = giver where the player never
-- handed it in; previous quests from the order the player got them; new hostile NPCs copy faction and stats from a related
-- NPC of ours (named in the comment), levels from the trace.
-- Quests that need a script, a credit NPC we lack, or a giver nobody saw are left out (list at the end).
-- Safe to run again: every key is deleted first; new spawns use guid blocks 47200000-47200999 (creature) and
-- 47210000-47210999 (gameobject).

-- A. NPCs we lacked (11)
DELETE FROM `creature` WHERE `guid` BETWEEN 47200000 AND 47200999;
-- 254560 Twilight Shadowmancer: copy of 256798 Twilight Neophyte, level 39-40 (trace)
DELETE FROM `creature_template` WHERE `entry`=254560;
DELETE FROM `creature_template_model` WHERE `CreatureID`=254560;
DELETE FROM `creature_template_difficulty` WHERE `Entry`=254560;
DELETE FROM `creature_classic_level` WHERE `entry`=254560;
INSERT INTO `creature_template` (`entry`,`KillCredit1`,`KillCredit2`,`name`,`femaleName`,`subname`,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,`npcflag`,`speed_walk`,`speed_run`,`scale`,`Classification`,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,`family`,`trainer_class`,`type`,`VehicleId`,`AIName`,`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,`ScriptName`,`StringId`,`VerifiedBuild`) SELECT 254560,0,0,'Twilight Shadowmancer',`femaleName`,NULL,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,0,`speed_walk`,`speed_run`,`scale`,0,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,0,`trainer_class`,7,`VehicleId`,'',`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,'',`StringId`,70862 FROM `creature_template` WHERE `entry`=256798;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES (254560,0,138242,1.0,1.0,70862),(254560,1,139814,1.0,1.0,70862);
INSERT INTO `creature_template_difficulty` (`Entry`,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,`LootID`,`PickPocketLootID`,`SkinLootID`,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild`) SELECT 254560,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,0,0,0,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild` FROM `creature_template_difficulty` WHERE `Entry`=256798;
INSERT INTO `creature_classic_level` (`entry`,`level_min`,`level_max`) VALUES (254560,39,40);
-- 255770 Redridge Mystic: copy of 255772 Redridge Trapper, level 36-37 (trace)
DELETE FROM `creature_template` WHERE `entry`=255770;
DELETE FROM `creature_template_model` WHERE `CreatureID`=255770;
DELETE FROM `creature_template_difficulty` WHERE `Entry`=255770;
DELETE FROM `creature_classic_level` WHERE `entry`=255770;
INSERT INTO `creature_template` (`entry`,`KillCredit1`,`KillCredit2`,`name`,`femaleName`,`subname`,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,`npcflag`,`speed_walk`,`speed_run`,`scale`,`Classification`,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,`family`,`trainer_class`,`type`,`VehicleId`,`AIName`,`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,`ScriptName`,`StringId`,`VerifiedBuild`) SELECT 255770,0,0,'Redridge Mystic',`femaleName`,NULL,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,0,`speed_walk`,`speed_run`,`scale`,0,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,0,`trainer_class`,7,`VehicleId`,'',`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,'',`StringId`,70862 FROM `creature_template` WHERE `entry`=255772;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES (255770,0,10789,1.0,1.0,70862);
INSERT INTO `creature_template_difficulty` (`Entry`,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,`LootID`,`PickPocketLootID`,`SkinLootID`,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild`) SELECT 255770,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,0,0,0,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild` FROM `creature_template_difficulty` WHERE `Entry`=255772;
INSERT INTO `creature_classic_level` (`entry`,`level_min`,`level_max`) VALUES (255770,36,37);
-- 257001 Old Rot-Chum: copy of 258574 Mudfin Warrior, level 42-42 (trace)
DELETE FROM `creature_template` WHERE `entry`=257001;
DELETE FROM `creature_template_model` WHERE `CreatureID`=257001;
DELETE FROM `creature_template_difficulty` WHERE `Entry`=257001;
DELETE FROM `creature_classic_level` WHERE `entry`=257001;
INSERT INTO `creature_template` (`entry`,`KillCredit1`,`KillCredit2`,`name`,`femaleName`,`subname`,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,`npcflag`,`speed_walk`,`speed_run`,`scale`,`Classification`,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,`family`,`trainer_class`,`type`,`VehicleId`,`AIName`,`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,`ScriptName`,`StringId`,`VerifiedBuild`) SELECT 257001,0,0,'Old Rot-Chum',`femaleName`,NULL,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,0,`speed_walk`,`speed_run`,`scale`,1,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,0,`trainer_class`,7,`VehicleId`,'',`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,'',`StringId`,70862 FROM `creature_template` WHERE `entry`=258574;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES (257001,0,5243,1.0,1.0,70862);
INSERT INTO `creature_template_difficulty` (`Entry`,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,`LootID`,`PickPocketLootID`,`SkinLootID`,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild`) SELECT 257001,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,0,0,0,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild` FROM `creature_template_difficulty` WHERE `Entry`=258574;
INSERT INTO `creature_classic_level` (`entry`,`level_min`,`level_max`) VALUES (257001,42,42);
-- 257558 Demar Brant <Knight Commander>: copy of 257591 Ashe Amberhall, level 55-55 (trace)
DELETE FROM `creature_template` WHERE `entry`=257558;
DELETE FROM `creature_template_model` WHERE `CreatureID`=257558;
DELETE FROM `creature_template_difficulty` WHERE `Entry`=257558;
DELETE FROM `creature_classic_level` WHERE `entry`=257558;
INSERT INTO `creature_template` (`entry`,`KillCredit1`,`KillCredit2`,`name`,`femaleName`,`subname`,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,`npcflag`,`speed_walk`,`speed_run`,`scale`,`Classification`,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,`family`,`trainer_class`,`type`,`VehicleId`,`AIName`,`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,`ScriptName`,`StringId`,`VerifiedBuild`) SELECT 257558,0,0,'Demar Brant',`femaleName`,'Knight Commander',`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,0,`speed_walk`,`speed_run`,`scale`,0,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,0,`trainer_class`,7,`VehicleId`,'',`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,'',`StringId`,70862 FROM `creature_template` WHERE `entry`=257591;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES (257558,0,140257,1.0,1.0,70862);
INSERT INTO `creature_template_difficulty` (`Entry`,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,`LootID`,`PickPocketLootID`,`SkinLootID`,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild`) SELECT 257558,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,0,0,0,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild` FROM `creature_template_difficulty` WHERE `Entry`=257591;
INSERT INTO `creature_classic_level` (`entry`,`level_min`,`level_max`) VALUES (257558,55,55);
-- 258522 Sootfur: copy of 259267 Plains Coyote, level 38-38 (trace)
DELETE FROM `creature_template` WHERE `entry`=258522;
DELETE FROM `creature_template_model` WHERE `CreatureID`=258522;
DELETE FROM `creature_template_difficulty` WHERE `Entry`=258522;
DELETE FROM `creature_classic_level` WHERE `entry`=258522;
INSERT INTO `creature_template` (`entry`,`KillCredit1`,`KillCredit2`,`name`,`femaleName`,`subname`,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,`npcflag`,`speed_walk`,`speed_run`,`scale`,`Classification`,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,`family`,`trainer_class`,`type`,`VehicleId`,`AIName`,`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,`ScriptName`,`StringId`,`VerifiedBuild`) SELECT 258522,0,0,'Sootfur',`femaleName`,NULL,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,0,`speed_walk`,`speed_run`,`scale`,0,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,1,`trainer_class`,1,`VehicleId`,'',`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,'',`StringId`,70862 FROM `creature_template` WHERE `entry`=259267;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES (258522,0,7339,1.0,1.0,70862);
INSERT INTO `creature_template_difficulty` (`Entry`,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,`LootID`,`PickPocketLootID`,`SkinLootID`,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild`) SELECT 258522,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,0,0,0,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild` FROM `creature_template_difficulty` WHERE `Entry`=259267;
INSERT INTO `creature_classic_level` (`entry`,`level_min`,`level_max`) VALUES (258522,38,38);
-- 258573 Mudfin Oracle: copy of 258574 Mudfin Warrior, level 36-37 (trace)
DELETE FROM `creature_template` WHERE `entry`=258573;
DELETE FROM `creature_template_model` WHERE `CreatureID`=258573;
DELETE FROM `creature_template_difficulty` WHERE `Entry`=258573;
DELETE FROM `creature_classic_level` WHERE `entry`=258573;
INSERT INTO `creature_template` (`entry`,`KillCredit1`,`KillCredit2`,`name`,`femaleName`,`subname`,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,`npcflag`,`speed_walk`,`speed_run`,`scale`,`Classification`,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,`family`,`trainer_class`,`type`,`VehicleId`,`AIName`,`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,`ScriptName`,`StringId`,`VerifiedBuild`) SELECT 258573,0,0,'Mudfin Oracle',`femaleName`,NULL,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,0,`speed_walk`,`speed_run`,`scale`,0,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,0,`trainer_class`,7,`VehicleId`,'',`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,'',`StringId`,70862 FROM `creature_template` WHERE `entry`=258574;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES (258573,0,2597,1.0,1.0,70862);
INSERT INTO `creature_template_difficulty` (`Entry`,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,`LootID`,`PickPocketLootID`,`SkinLootID`,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild`) SELECT 258573,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,0,0,0,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild` FROM `creature_template_difficulty` WHERE `Entry`=258574;
INSERT INTO `creature_classic_level` (`entry`,`level_min`,`level_max`) VALUES (258573,36,37);
-- 259051 Watcher Fredericks: copy of 257591 Ashe Amberhall, level 35-35 (trace)
DELETE FROM `creature_template` WHERE `entry`=259051;
DELETE FROM `creature_template_model` WHERE `CreatureID`=259051;
DELETE FROM `creature_template_difficulty` WHERE `Entry`=259051;
DELETE FROM `creature_classic_level` WHERE `entry`=259051;
INSERT INTO `creature_template` (`entry`,`KillCredit1`,`KillCredit2`,`name`,`femaleName`,`subname`,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,`npcflag`,`speed_walk`,`speed_run`,`scale`,`Classification`,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,`family`,`trainer_class`,`type`,`VehicleId`,`AIName`,`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,`ScriptName`,`StringId`,`VerifiedBuild`) SELECT 259051,0,0,'Watcher Fredericks',`femaleName`,NULL,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,0,`speed_walk`,`speed_run`,`scale`,0,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,0,`trainer_class`,7,`VehicleId`,'',`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,'',`StringId`,70862 FROM `creature_template` WHERE `entry`=257591;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES (259051,0,141301,1.0,1.0,70862);
INSERT INTO `creature_template_difficulty` (`Entry`,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,`LootID`,`PickPocketLootID`,`SkinLootID`,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild`) SELECT 259051,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,0,0,0,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild` FROM `creature_template_difficulty` WHERE `Entry`=257591;
INSERT INTO `creature_classic_level` (`entry`,`level_min`,`level_max`) VALUES (259051,35,35);
-- 259158 Fuzzle: copy of 257591 Ashe Amberhall, level 38-38 (trace)
DELETE FROM `creature_template` WHERE `entry`=259158;
DELETE FROM `creature_template_model` WHERE `CreatureID`=259158;
DELETE FROM `creature_template_difficulty` WHERE `Entry`=259158;
DELETE FROM `creature_classic_level` WHERE `entry`=259158;
INSERT INTO `creature_template` (`entry`,`KillCredit1`,`KillCredit2`,`name`,`femaleName`,`subname`,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,`npcflag`,`speed_walk`,`speed_run`,`scale`,`Classification`,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,`family`,`trainer_class`,`type`,`VehicleId`,`AIName`,`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,`ScriptName`,`StringId`,`VerifiedBuild`) SELECT 259158,0,0,'Fuzzle',`femaleName`,NULL,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,0,`speed_walk`,`speed_run`,`scale`,0,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,0,`trainer_class`,7,`VehicleId`,'',`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,'',`StringId`,70862 FROM `creature_template` WHERE `entry`=257591;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES (259158,0,269,1.0,1.0,70862);
INSERT INTO `creature_template_difficulty` (`Entry`,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,`LootID`,`PickPocketLootID`,`SkinLootID`,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild`) SELECT 259158,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,0,0,0,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild` FROM `creature_template_difficulty` WHERE `Entry`=257591;
INSERT INTO `creature_classic_level` (`entry`,`level_min`,`level_max`) VALUES (259158,38,38);
-- 262399 Gelinda Coppergleam: copy of 257591 Ashe Amberhall, level 40-40 (trace)
DELETE FROM `creature_template` WHERE `entry`=262399;
DELETE FROM `creature_template_model` WHERE `CreatureID`=262399;
DELETE FROM `creature_template_difficulty` WHERE `Entry`=262399;
DELETE FROM `creature_classic_level` WHERE `entry`=262399;
INSERT INTO `creature_template` (`entry`,`KillCredit1`,`KillCredit2`,`name`,`femaleName`,`subname`,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,`npcflag`,`speed_walk`,`speed_run`,`scale`,`Classification`,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,`family`,`trainer_class`,`type`,`VehicleId`,`AIName`,`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,`ScriptName`,`StringId`,`VerifiedBuild`) SELECT 262399,0,0,'Gelinda Coppergleam',`femaleName`,NULL,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,0,`speed_walk`,`speed_run`,`scale`,0,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,0,`trainer_class`,7,`VehicleId`,'',`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,'',`StringId`,70862 FROM `creature_template` WHERE `entry`=257591;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES (262399,0,143169,1.0,1.0,70862);
INSERT INTO `creature_template_difficulty` (`Entry`,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,`LootID`,`PickPocketLootID`,`SkinLootID`,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild`) SELECT 262399,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,0,0,0,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild` FROM `creature_template_difficulty` WHERE `Entry`=257591;
INSERT INTO `creature_classic_level` (`entry`,`level_min`,`level_max`) VALUES (262399,40,40);
-- 262473 Cinderscale Whelp: copy of 259267 Plains Coyote, level 39-40 (trace)
DELETE FROM `creature_template` WHERE `entry`=262473;
DELETE FROM `creature_template_model` WHERE `CreatureID`=262473;
DELETE FROM `creature_template_difficulty` WHERE `Entry`=262473;
DELETE FROM `creature_classic_level` WHERE `entry`=262473;
INSERT INTO `creature_template` (`entry`,`KillCredit1`,`KillCredit2`,`name`,`femaleName`,`subname`,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,`npcflag`,`speed_walk`,`speed_run`,`scale`,`Classification`,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,`family`,`trainer_class`,`type`,`VehicleId`,`AIName`,`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,`ScriptName`,`StringId`,`VerifiedBuild`) SELECT 262473,0,0,'Cinderscale Whelp',`femaleName`,NULL,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,0,`speed_walk`,`speed_run`,`scale`,0,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,0,`trainer_class`,2,`VehicleId`,'',`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,'',`StringId`,70862 FROM `creature_template` WHERE `entry`=259267;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES (262473,0,497,1.0,1.0,70862);
INSERT INTO `creature_template_difficulty` (`Entry`,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,`LootID`,`PickPocketLootID`,`SkinLootID`,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild`) SELECT 262473,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,0,0,0,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild` FROM `creature_template_difficulty` WHERE `Entry`=259267;
INSERT INTO `creature_classic_level` (`entry`,`level_min`,`level_max`) VALUES (262473,39,40);
-- 262477 Cinderscale Warrior: copy of 259267 Plains Coyote, level 41-42 (trace)
DELETE FROM `creature_template` WHERE `entry`=262477;
DELETE FROM `creature_template_model` WHERE `CreatureID`=262477;
DELETE FROM `creature_template_difficulty` WHERE `Entry`=262477;
DELETE FROM `creature_classic_level` WHERE `entry`=262477;
INSERT INTO `creature_template` (`entry`,`KillCredit1`,`KillCredit2`,`name`,`femaleName`,`subname`,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,`npcflag`,`speed_walk`,`speed_run`,`scale`,`Classification`,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,`family`,`trainer_class`,`type`,`VehicleId`,`AIName`,`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,`ScriptName`,`StringId`,`VerifiedBuild`) SELECT 262477,0,0,'Cinderscale Warrior',`femaleName`,NULL,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,0,`speed_walk`,`speed_run`,`scale`,0,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,0,`trainer_class`,2,`VehicleId`,'',`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,'',`StringId`,70862 FROM `creature_template` WHERE `entry`=259267;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES (262477,0,143,1.0,0.0,70862);
INSERT INTO `creature_template_difficulty` (`Entry`,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,`LootID`,`PickPocketLootID`,`SkinLootID`,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild`) SELECT 262477,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,0,0,0,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild` FROM `creature_template_difficulty` WHERE `Entry`=259267;
INSERT INTO `creature_classic_level` (`entry`,`level_min`,`level_max`) VALUES (262477,41,42);
INSERT INTO `creature` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`modelid`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`currentwaypoint`,`curHealthPct`,`MovementType`,`npcflag`,`unit_flags`,`unit_flags2`,`unit_flags3`,`ScriptName`,`StringId`,`VerifiedBuild`) VALUES
(47200000,254560,0,0,0,'0',0,0,0,-1,0,0,-8077.92,-4208.93,27.70,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200001,254560,0,0,0,'0',0,0,0,-1,0,0,-8080.50,-4160.36,27.36,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200002,254560,0,0,0,'0',0,0,0,-1,0,0,-8107.98,-4168.30,27.24,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200003,254560,0,0,0,'0',0,0,0,-1,0,0,-8120.59,-4181.00,20.45,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200004,254560,0,0,0,'0',0,0,0,-1,0,0,-8062.73,-4247.23,27.91,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200005,254560,0,0,0,'0',0,0,0,-1,0,0,-8174.25,-4084.62,9.87,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200006,255770,0,0,0,'0',0,0,0,-1,0,0,-8446.10,-3362.69,170.87,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200007,255770,0,0,0,'0',0,0,0,-1,0,0,-8642.66,-3385.72,172.30,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200008,255770,0,0,0,'0',0,0,0,-1,0,0,-8598.69,-3344.64,172.43,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200009,255770,0,0,0,'0',0,0,0,-1,0,0,-8547.29,-3377.38,172.09,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200010,255770,0,0,0,'0',0,0,0,-1,0,0,-8508.50,-3420.40,171.63,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200011,255770,0,0,0,'0',0,0,0,-1,0,0,-8481.02,-3454.74,171.47,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200012,255770,0,0,0,'0',0,0,0,-1,0,0,-8291.56,-3379.26,171.35,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200013,255770,0,0,0,'0',0,0,0,-1,0,0,-8360.74,-3301.93,171.49,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200014,255770,0,0,0,'0',0,0,0,-1,0,0,-8420.89,-3430.08,170.97,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200015,255770,0,0,0,'0',0,0,0,-1,0,0,-8615.83,-3411.80,172.17,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200016,257001,0,0,0,'0',0,0,0,-1,0,0,-8848.75,-5634.37,-3.75,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200017,257558,0,0,0,'0',0,0,0,-1,0,0,-9080.52,-5119.68,49.44,0,120,0,0,0,0,NULL,NULL,NULL,NULL,'',NULL,0),
(47200018,258522,0,0,0,'0',0,0,0,-1,0,0,-9222.41,-4553.93,17.11,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200019,258573,0,0,0,'0',0,0,0,-1,0,0,-9035.21,-4383.72,19.62,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200020,258573,0,0,0,'0',0,0,0,-1,0,0,-9074.00,-4364.51,19.89,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200021,258573,0,0,0,'0',0,0,0,-1,0,0,-9001.91,-4343.19,21.34,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200022,258573,0,0,0,'0',0,0,0,-1,0,0,-8970.55,-4322.11,21.87,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200023,258573,0,0,0,'0',0,0,0,-1,0,0,-8933.04,-4345.70,22.27,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200024,258573,0,0,0,'0',0,0,0,-1,0,0,-8925.61,-4326.21,22.28,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200025,258573,0,0,0,'0',0,0,0,-1,0,0,-8992.85,-4314.47,21.73,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200026,258573,0,0,0,'0',0,0,0,-1,0,0,-8967.95,-4248.72,25.61,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200027,258573,0,0,0,'0',0,0,0,-1,0,0,-8992.52,-4244.49,25.59,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200028,258573,0,0,0,'0',0,0,0,-1,0,0,-9004.81,-4284.41,21.97,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200029,258573,0,0,0,'0',0,0,0,-1,0,0,-9039.08,-4267.12,22.73,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200030,258573,0,0,0,'0',0,0,0,-1,0,0,-9064.30,-4259.98,23.32,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200031,258573,0,0,0,'0',0,0,0,-1,0,0,-9043.29,-4294.84,22.03,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200032,258573,0,0,0,'0',0,0,0,-1,0,0,-8994.79,-4330.51,21.56,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200033,258573,0,0,0,'0',0,0,0,-1,0,0,-8890.37,-4276.93,22.82,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200034,258573,0,0,0,'0',0,0,0,-1,0,0,-8349.47,-4008.40,13.75,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200035,258573,0,0,0,'0',0,0,0,-1,0,0,-8390.85,-3968.80,18.05,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200036,258573,0,0,0,'0',0,0,0,-1,0,0,-8371.46,-4007.07,15.69,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200037,258573,0,0,0,'0',0,0,0,-1,0,0,-8353.36,-4030.29,13.49,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200038,258573,0,0,0,'0',0,0,0,-1,0,0,-8329.11,-4043.27,13.15,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200039,258573,0,0,0,'0',0,0,0,-1,0,0,-8325.55,-4004.37,13.53,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200040,258573,0,0,0,'0',0,0,0,-1,0,0,-8342.04,-3996.21,13.91,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200041,258573,0,0,0,'0',0,0,0,-1,0,0,-8394.09,-4036.36,15.94,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200042,258573,0,0,0,'0',0,0,0,-1,0,0,-9010.63,-4273.75,22.09,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200043,258573,0,0,0,'0',0,0,0,-1,0,0,-8844.78,-4307.27,26.32,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200044,259051,0,0,0,'0',0,0,0,-1,0,0,-8916.19,-3655.58,51.43,0,120,0,0,0,0,NULL,NULL,NULL,NULL,'',NULL,0),
(47200045,259158,0,0,0,'0',0,0,0,-1,0,0,-8120.92,-4179.55,20.44,0,120,0,0,0,0,NULL,NULL,NULL,NULL,'',NULL,0),
(47200046,262399,0,0,0,'0',0,0,0,-1,0,0,-8688.25,-3352.95,164.20,0,120,0,0,0,0,NULL,NULL,NULL,NULL,'',NULL,0),
(47200047,262473,0,0,0,'0',0,0,0,-1,0,0,-8791.05,-3323.43,110.46,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200048,262473,0,0,0,'0',0,0,0,-1,0,0,-8802.37,-3295.31,110.46,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200049,262473,0,0,0,'0',0,0,0,-1,0,0,-8786.20,-3293.27,110.46,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200050,262473,0,0,0,'0',0,0,0,-1,0,0,-8824.03,-3307.10,110.46,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200051,262473,0,0,0,'0',0,0,0,-1,0,0,-8849.25,-3373.83,110.46,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200052,262473,0,0,0,'0',0,0,0,-1,0,0,-8794.93,-3342.89,110.46,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200053,262477,0,0,0,'0',0,0,0,-1,0,0,-8821.45,-3372.69,110.46,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200054,255772,0,0,0,'0',0,0,0,-1,0,0,-8619.71,-3404.54,172.20,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200055,255772,0,0,0,'0',0,0,0,-1,0,0,-8524.34,-3394.25,171.84,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200056,255772,0,0,0,'0',0,0,0,-1,0,0,-8481.67,-3469.32,171.51,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200057,255772,0,0,0,'0',0,0,0,-1,0,0,-8432.52,-3427.24,170.98,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200058,255772,0,0,0,'0',0,0,0,-1,0,0,-8329.07,-3419.33,171.29,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200059,255772,0,0,0,'0',0,0,0,-1,0,0,-8295.77,-3409.42,158.83,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200060,255772,0,0,0,'0',0,0,0,-1,0,0,-8358.48,-3323.78,171.44,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200061,255772,0,0,0,'0',0,0,0,-1,0,0,-8383.70,-3336.08,171.40,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200062,255898,0,0,0,'0',0,0,0,-1,0,0,-8456.57,-5338.54,16.39,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200063,255898,0,0,0,'0',0,0,0,-1,0,0,-8962.19,-5176.32,14.83,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200064,255898,0,0,0,'0',0,0,0,-1,0,0,-8863.91,-5118.40,6.13,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200065,255898,0,0,0,'0',0,0,0,-1,0,0,-8671.24,-5336.41,6.77,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200066,255898,0,0,0,'0',0,0,0,-1,0,0,-8582.34,-5502.07,5.78,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200067,255898,0,0,0,'0',0,0,0,-1,0,0,-8598.51,-5515.29,8.41,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200068,255898,0,0,0,'0',0,0,0,-1,0,0,-8689.35,-5516.31,9.65,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200069,255898,0,0,0,'0',0,0,0,-1,0,0,-8851.01,-5631.95,-3.59,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200070,255899,0,0,0,'0',0,0,0,-1,0,0,-8967.69,-5177.33,14.90,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200071,255899,0,0,0,'0',0,0,0,-1,0,0,-8826.73,-5181.83,6.45,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200072,255899,0,0,0,'0',0,0,0,-1,0,0,-8784.71,-5256.90,7.74,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200073,255899,0,0,0,'0',0,0,0,-1,0,0,-8727.17,-5293.01,9.29,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200074,255899,0,0,0,'0',0,0,0,-1,0,0,-8750.78,-5458.36,8.07,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200075,255901,0,0,0,'0',0,0,0,-1,0,0,-8853.87,-4924.45,6.06,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200076,255901,0,0,0,'0',0,0,0,-1,0,0,-8924.03,-4852.95,15.77,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200077,255901,0,0,0,'0',0,0,0,-1,0,0,-8945.06,-5132.00,21.19,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200078,255901,0,0,0,'0',0,0,0,-1,0,0,-8870.70,-5092.20,5.47,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200079,255901,0,0,0,'0',0,0,0,-1,0,0,-8840.95,-4996.78,4.21,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200080,255901,0,0,0,'0',0,0,0,-1,0,0,-8950.22,-5020.27,28.57,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200081,255901,0,0,0,'0',0,0,0,-1,0,0,-8811.86,-5210.90,6.46,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200082,255901,0,0,0,'0',0,0,0,-1,0,0,-8795.70,-5251.14,7.75,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200083,256801,0,0,0,'0',0,0,0,-1,0,0,-8762.73,-5278.15,7.47,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200084,256801,0,0,0,'0',0,0,0,-1,0,0,-8668.98,-5332.99,7.16,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200085,256801,0,0,0,'0',0,0,0,-1,0,0,-8597.54,-5500.70,6.08,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200086,256801,0,0,0,'0',0,0,0,-1,0,0,-8641.83,-5564.14,5.01,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200087,256801,0,0,0,'0',0,0,0,-1,0,0,-8714.57,-5497.02,12.65,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200088,256801,0,0,0,'0',0,0,0,-1,0,0,-8654.44,-5463.62,8.26,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200089,256801,0,0,0,'0',0,0,0,-1,0,0,-8606.27,-5486.66,5.93,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200090,256801,0,0,0,'0',0,0,0,-1,0,0,-8808.66,-5579.22,0.90,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200091,256952,0,0,0,'0',0,0,0,-1,0,0,-8299.06,-4384.21,28.94,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200092,256952,0,0,0,'0',0,0,0,-1,0,0,-8320.08,-4375.59,29.17,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200093,256952,0,0,0,'0',0,0,0,-1,0,0,-8306.18,-4355.10,29.02,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200094,256952,0,0,0,'0',0,0,0,-1,0,0,-8347.88,-4357.29,29.38,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200095,256952,0,0,0,'0',0,0,0,-1,0,0,-8376.01,-4349.68,29.51,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200096,256952,0,0,0,'0',0,0,0,-1,0,0,-8345.62,-4329.58,29.22,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200097,256952,0,0,0,'0',0,0,0,-1,0,0,-8293.89,-4315.66,28.91,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200098,256952,0,0,0,'0',0,0,0,-1,0,0,-8143.88,-4278.33,24.33,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200099,258574,0,0,0,'0',0,0,0,-1,0,0,-9046.85,-4380.39,19.36,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200100,258574,0,0,0,'0',0,0,0,-1,0,0,-8993.82,-4344.11,21.46,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200101,258574,0,0,0,'0',0,0,0,-1,0,0,-8976.69,-4325.55,21.79,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200102,258574,0,0,0,'0',0,0,0,-1,0,0,-8895.22,-4338.18,26.58,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200103,258574,0,0,0,'0',0,0,0,-1,0,0,-8933.69,-4356.39,22.33,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200104,258574,0,0,0,'0',0,0,0,-1,0,0,-8926.57,-4249.44,25.62,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200105,258574,0,0,0,'0',0,0,0,-1,0,0,-8330.40,-3978.64,14.17,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200106,258574,0,0,0,'0',0,0,0,-1,0,0,-8395.38,-3982.43,18.92,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200107,258588,0,0,0,'0',0,0,0,-1,0,0,-8608.11,-3920.04,14.99,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200108,258588,0,0,0,'0',0,0,0,-1,0,0,-8719.33,-4129.16,18.34,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200109,258588,0,0,0,'0',0,0,0,-1,0,0,-8427.39,-4075.43,16.41,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200110,258588,0,0,0,'0',0,0,0,-1,0,0,-8246.36,-4201.67,10.09,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200111,258588,0,0,0,'0',0,0,0,-1,0,0,-8204.33,-4275.29,18.60,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200112,258588,0,0,0,'0',0,0,0,-1,0,0,-9124.77,-4496.50,13.08,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200113,258588,0,0,0,'0',0,0,0,-1,0,0,-9115.40,-4575.65,9.09,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200114,259267,0,0,0,'0',0,0,0,-1,0,0,-9174.88,-4560.94,7.84,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200115,259267,0,0,0,'0',0,0,0,-1,0,0,-9201.39,-4564.98,14.67,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200116,259267,0,0,0,'0',0,0,0,-1,0,0,-9239.22,-4555.00,18.56,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200117,259267,0,0,0,'0',0,0,0,-1,0,0,-9222.73,-4553.45,17.14,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200118,259267,0,0,0,'0',0,0,0,-1,0,0,-9103.76,-4542.05,11.93,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200119,259267,0,0,0,'0',0,0,0,-1,0,0,-9200.42,-4539.22,9.80,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200120,259267,0,0,0,'0',0,0,0,-1,0,0,-9271.23,-4563.46,33.35,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0),
(47200121,259267,0,0,0,'0',0,0,0,-1,0,0,-9171.65,-4488.52,12.25,0,300,5,0,0,1,NULL,NULL,NULL,NULL,'',NULL,0);

-- B. Objects we lacked (11): templates as the cache has them, spots where the player looted them or took the quest
DELETE FROM `gameobject` WHERE `guid` BETWEEN 47210000 AND 47210999;
-- 623246 Twilight Page: the cache has no loot id for it (Blizzard gives the item another way); ours is 623246
DELETE FROM `gameobject_template` WHERE `entry`=623246;
DELETE FROM `gameobject_template_addon` WHERE `entry`=623246;
INSERT INTO `gameobject_template` (`entry`,`type`,`displayId`,`name`,`IconName`,`castBarCaption`,`unk1`,`size`,`Data0`,`Data1`,`Data2`,`Data3`,`Data4`,`Data5`,`Data6`,`Data7`,`Data8`,`Data9`,`Data10`,`Data11`,`Data12`,`Data13`,`Data14`,`Data15`,`Data16`,`Data17`,`Data18`,`Data19`,`Data20`,`Data21`,`Data22`,`Data23`,`Data24`,`Data25`,`Data26`,`Data27`,`Data28`,`Data29`,`Data30`,`Data31`,`Data32`,`Data33`,`Data34`,`ContentTuningId`,`RequiredLevel`,`VerifiedBuild`) VALUES (623246,3,183,'Twilight Page','','','',0.75,4336,623246,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4097,0,0,0,0,0,0,0,0,0,138939,0,0,0,0,5841,1,70862);
-- 624202 Hollow Clam Shell
DELETE FROM `gameobject_template` WHERE `entry`=624202;
DELETE FROM `gameobject_template_addon` WHERE `entry`=624202;
INSERT INTO `gameobject_template` (`entry`,`type`,`displayId`,`name`,`IconName`,`castBarCaption`,`unk1`,`size`,`Data0`,`Data1`,`Data2`,`Data3`,`Data4`,`Data5`,`Data6`,`Data7`,`Data8`,`Data9`,`Data10`,`Data11`,`Data12`,`Data13`,`Data14`,`Data15`,`Data16`,`Data17`,`Data18`,`Data19`,`Data20`,`Data21`,`Data22`,`Data23`,`Data24`,`Data25`,`Data26`,`Data27`,`Data28`,`Data29`,`Data30`,`Data31`,`Data32`,`Data33`,`Data34`,`ContentTuningId`,`RequiredLevel`,`VerifiedBuild`) VALUES (624202,3,261,'Hollow Clam Shell','','','',0.75,93,138950,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,4099,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5841,1,70862);
-- 624265 Stolen Tools
DELETE FROM `gameobject_template` WHERE `entry`=624265;
DELETE FROM `gameobject_template_addon` WHERE `entry`=624265;
INSERT INTO `gameobject_template` (`entry`,`type`,`displayId`,`name`,`IconName`,`castBarCaption`,`unk1`,`size`,`Data0`,`Data1`,`Data2`,`Data3`,`Data4`,`Data5`,`Data6`,`Data7`,`Data8`,`Data9`,`Data10`,`Data11`,`Data12`,`Data13`,`Data14`,`Data15`,`Data16`,`Data17`,`Data18`,`Data19`,`Data20`,`Data21`,`Data22`,`Data23`,`Data24`,`Data25`,`Data26`,`Data27`,`Data28`,`Data29`,`Data30`,`Data31`,`Data32`,`Data33`,`Data34`,`ContentTuningId`,`RequiredLevel`,`VerifiedBuild`) VALUES (624265,3,31,'Stolen Tools','','','',1.0,93,138957,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,4099,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5841,1,70862);
-- 626594 Fresh Manure
DELETE FROM `gameobject_template` WHERE `entry`=626594;
DELETE FROM `gameobject_template_addon` WHERE `entry`=626594;
INSERT INTO `gameobject_template` (`entry`,`type`,`displayId`,`name`,`IconName`,`castBarCaption`,`unk1`,`size`,`Data0`,`Data1`,`Data2`,`Data3`,`Data4`,`Data5`,`Data6`,`Data7`,`Data8`,`Data9`,`Data10`,`Data11`,`Data12`,`Data13`,`Data14`,`Data15`,`Data16`,`Data17`,`Data18`,`Data19`,`Data20`,`Data21`,`Data22`,`Data23`,`Data24`,`Data25`,`Data26`,`Data27`,`Data28`,`Data29`,`Data30`,`Data31`,`Data32`,`Data33`,`Data34`,`ContentTuningId`,`RequiredLevel`,`VerifiedBuild`) VALUES (626594,3,125071,'Fresh Manure','','','',0.5,43,139130,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,4099,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5841,1,70862);
-- 628948 WANTED!
DELETE FROM `gameobject_template` WHERE `entry`=628948;
DELETE FROM `gameobject_template_addon` WHERE `entry`=628948;
INSERT INTO `gameobject_template` (`entry`,`type`,`displayId`,`name`,`IconName`,`castBarCaption`,`unk1`,`size`,`Data0`,`Data1`,`Data2`,`Data3`,`Data4`,`Data5`,`Data6`,`Data7`,`Data8`,`Data9`,`Data10`,`Data11`,`Data12`,`Data13`,`Data14`,`Data15`,`Data16`,`Data17`,`Data18`,`Data19`,`Data20`,`Data21`,`Data22`,`Data23`,`Data24`,`Data25`,`Data26`,`Data27`,`Data28`,`Data29`,`Data30`,`Data31`,`Data32`,`Data33`,`Data34`,`ContentTuningId`,`RequiredLevel`,`VerifiedBuild`) VALUES (628948,2,17,'WANTED!','','','',1.0,0,29533,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70862);
-- 629484 Hay Bale
DELETE FROM `gameobject_template` WHERE `entry`=629484;
DELETE FROM `gameobject_template_addon` WHERE `entry`=629484;
INSERT INTO `gameobject_template` (`entry`,`type`,`displayId`,`name`,`IconName`,`castBarCaption`,`unk1`,`size`,`Data0`,`Data1`,`Data2`,`Data3`,`Data4`,`Data5`,`Data6`,`Data7`,`Data8`,`Data9`,`Data10`,`Data11`,`Data12`,`Data13`,`Data14`,`Data15`,`Data16`,`Data17`,`Data18`,`Data19`,`Data20`,`Data21`,`Data22`,`Data23`,`Data24`,`Data25`,`Data26`,`Data27`,`Data28`,`Data29`,`Data30`,`Data31`,`Data32`,`Data33`,`Data34`,`ContentTuningId`,`RequiredLevel`,`VerifiedBuild`) VALUES (629484,3,122141,'Hay Bale','','','',0.5,43,139561,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4097,0,0,0,0,0,0,0,0,0,0,0,0,0,0,181,1,70862);
-- 645233 Fresh Apple
DELETE FROM `gameobject_template` WHERE `entry`=645233;
DELETE FROM `gameobject_template_addon` WHERE `entry`=645233;
INSERT INTO `gameobject_template` (`entry`,`type`,`displayId`,`name`,`IconName`,`castBarCaption`,`unk1`,`size`,`Data0`,`Data1`,`Data2`,`Data3`,`Data4`,`Data5`,`Data6`,`Data7`,`Data8`,`Data9`,`Data10`,`Data11`,`Data12`,`Data13`,`Data14`,`Data15`,`Data16`,`Data17`,`Data18`,`Data19`,`Data20`,`Data21`,`Data22`,`Data23`,`Data24`,`Data25`,`Data26`,`Data27`,`Data28`,`Data29`,`Data30`,`Data31`,`Data32`,`Data33`,`Data34`,`ContentTuningId`,`RequiredLevel`,`VerifiedBuild`) VALUES (645233,3,433,'Fresh Apple','','','',1.0,4386,140443,60000,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4097,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5841,1,70862);
-- 645449 Foreman's Crate
DELETE FROM `gameobject_template` WHERE `entry`=645449;
DELETE FROM `gameobject_template_addon` WHERE `entry`=645449;
INSERT INTO `gameobject_template` (`entry`,`type`,`displayId`,`name`,`IconName`,`castBarCaption`,`unk1`,`size`,`Data0`,`Data1`,`Data2`,`Data3`,`Data4`,`Data5`,`Data6`,`Data7`,`Data8`,`Data9`,`Data10`,`Data11`,`Data12`,`Data13`,`Data14`,`Data15`,`Data16`,`Data17`,`Data18`,`Data19`,`Data20`,`Data21`,`Data22`,`Data23`,`Data24`,`Data25`,`Data26`,`Data27`,`Data28`,`Data29`,`Data30`,`Data31`,`Data32`,`Data33`,`Data34`,`ContentTuningId`,`RequiredLevel`,`VerifiedBuild`) VALUES (645449,3,336,'Foreman''s Crate','','','',1.0,4387,140475,60000,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4097,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5841,1,70862);
-- 645516 Bundle of Wood
DELETE FROM `gameobject_template` WHERE `entry`=645516;
DELETE FROM `gameobject_template_addon` WHERE `entry`=645516;
INSERT INTO `gameobject_template` (`entry`,`type`,`displayId`,`name`,`IconName`,`castBarCaption`,`unk1`,`size`,`Data0`,`Data1`,`Data2`,`Data3`,`Data4`,`Data5`,`Data6`,`Data7`,`Data8`,`Data9`,`Data10`,`Data11`,`Data12`,`Data13`,`Data14`,`Data15`,`Data16`,`Data17`,`Data18`,`Data19`,`Data20`,`Data21`,`Data22`,`Data23`,`Data24`,`Data25`,`Data26`,`Data27`,`Data28`,`Data29`,`Data30`,`Data31`,`Data32`,`Data33`,`Data34`,`ContentTuningId`,`RequiredLevel`,`VerifiedBuild`) VALUES (645516,3,1248,'Bundle of Wood','','','',1.0,4388,140476,60000,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4097,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5841,1,70862);
-- 648590 Cubic Draconia
DELETE FROM `gameobject_template` WHERE `entry`=648590;
DELETE FROM `gameobject_template_addon` WHERE `entry`=648590;
INSERT INTO `gameobject_template` (`entry`,`type`,`displayId`,`name`,`IconName`,`castBarCaption`,`unk1`,`size`,`Data0`,`Data1`,`Data2`,`Data3`,`Data4`,`Data5`,`Data6`,`Data7`,`Data8`,`Data9`,`Data10`,`Data11`,`Data12`,`Data13`,`Data14`,`Data15`,`Data16`,`Data17`,`Data18`,`Data19`,`Data20`,`Data21`,`Data22`,`Data23`,`Data24`,`Data25`,`Data26`,`Data27`,`Data28`,`Data29`,`Data30`,`Data31`,`Data32`,`Data33`,`Data34`,`ContentTuningId`,`RequiredLevel`,`VerifiedBuild`) VALUES (648590,3,124262,'Cubic Draconia','','','',1.0,43,140524,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4097,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5841,1,70862);
-- 649232 Marsh Gas Vent
DELETE FROM `gameobject_template` WHERE `entry`=649232;
DELETE FROM `gameobject_template_addon` WHERE `entry`=649232;
INSERT INTO `gameobject_template` (`entry`,`type`,`displayId`,`name`,`IconName`,`castBarCaption`,`unk1`,`size`,`Data0`,`Data1`,`Data2`,`Data3`,`Data4`,`Data5`,`Data6`,`Data7`,`Data8`,`Data9`,`Data10`,`Data11`,`Data12`,`Data13`,`Data14`,`Data15`,`Data16`,`Data17`,`Data18`,`Data19`,`Data20`,`Data21`,`Data22`,`Data23`,`Data24`,`Data25`,`Data26`,`Data27`,`Data28`,`Data29`,`Data30`,`Data31`,`Data32`,`Data33`,`Data34`,`ContentTuningId`,`RequiredLevel`,`VerifiedBuild`) VALUES (649232,3,124377,'Marsh Gas Vent','','','',0.33,43,140588,0,0,0,1,0,0,0,0,1,0,0,0,0,0,0,0,0,0,4099,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5841,1,70862);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`,`ScriptName`,`StringId`,`VerifiedBuild`) VALUES
(47210000,623246,0,0,0,'0',0,0,0,-1,-8120.59,-4173.23,18.44,0,0,0,0,1,60,255,1,'',NULL,0),
(47210001,623246,0,0,0,'0',0,0,0,-1,-8149.36,-4129.18,12.65,0,0,0,0,1,60,255,1,'',NULL,0),
(47210002,623246,0,0,0,'0',0,0,0,-1,-8092.78,-4027.29,14.45,0,0,0,0,1,60,255,1,'',NULL,0),
(47210003,623246,0,0,0,'0',0,0,0,-1,-8140.63,-4065.47,9.90,0,0,0,0,1,60,255,1,'',NULL,0),
(47210004,623246,0,0,0,'0',0,0,0,-1,-8169.41,-4136.10,12.33,0,0,0,0,1,60,255,1,'',NULL,0),
(47210005,624202,0,0,0,'0',0,0,0,-1,-9007.72,-4280.05,19.92,0,0,0,0,1,60,255,1,'',NULL,0),
(47210006,626594,0,0,0,'0',0,0,0,-1,-9052.97,-4087.41,23.15,0,0,0,0,1,60,255,1,'',NULL,0),
(47210007,626594,0,0,0,'0',0,0,0,-1,-9104.70,-4070.22,31.83,0,0,0,0,1,60,255,1,'',NULL,0),
(47210008,626594,0,0,0,'0',0,0,0,-1,-9084.65,-4089.54,23.28,0,0,0,0,1,60,255,1,'',NULL,0),
(47210009,626594,0,0,0,'0',0,0,0,-1,-9062.35,-4146.26,23.45,0,0,0,0,1,60,255,1,'',NULL,0),
(47210010,628948,0,0,0,'0',0,0,0,-1,-8205.72,-5694.21,5.80,0,0,0,0,1,300,255,1,'',NULL,0),
(47210011,629484,0,0,0,'0',0,0,0,-1,-8871.63,-4574.69,27.73,0,0,0,0,1,60,255,1,'',NULL,0),
(47210012,629484,0,0,0,'0',0,0,0,-1,-8875.51,-4544.59,29.00,0,0,0,0,1,60,255,1,'',NULL,0),
(47210013,629484,0,0,0,'0',0,0,0,-1,-8896.85,-4540.34,28.99,0,0,0,0,1,60,255,1,'',NULL,0),
(47210014,645233,0,0,0,'0',0,0,0,-1,-9097.33,-5073.62,38.90,0,0,0,0,1,60,255,1,'',NULL,0),
(47210015,645233,0,0,0,'0',0,0,0,-1,-9097.33,-5068.28,38.86,0,0,0,0,1,60,255,1,'',NULL,0),
(47210016,645233,0,0,0,'0',0,0,0,-1,-9172.64,-4941.41,39.68,0,0,0,0,1,60,255,1,'',NULL,0),
(47210017,645233,0,0,0,'0',0,0,0,-1,-9153.25,-5033.13,37.81,0,0,0,0,1,60,255,1,'',NULL,0),
(47210018,645233,0,0,0,'0',0,0,0,-1,-9156.49,-5026.35,37.68,0,0,0,0,1,60,255,1,'',NULL,0),
(47210019,645233,0,0,0,'0',0,0,0,-1,-9170.06,-4953.06,37.44,0,0,0,0,1,60,255,1,'',NULL,0),
(47210020,645449,0,0,0,'0',0,0,0,-1,-8314.91,-4368.75,27.11,0,0,0,0,1,60,255,1,'',NULL,0),
(47210021,645516,0,0,0,'0',0,0,0,-1,-8559.29,-3892.54,12.71,0,0,0,0,1,60,255,1,'',NULL,0),
(47210022,645516,0,0,0,'0',0,0,0,-1,-8588.05,-3764.42,13.09,0,0,0,0,1,60,255,1,'',NULL,0),
(47210023,645516,0,0,0,'0',0,0,0,-1,-8442.89,-3706.22,16.08,0,0,0,0,1,60,255,1,'',NULL,0),
(47210024,645516,0,0,0,'0',0,0,0,-1,-8572.53,-3773.56,13.10,0,0,0,0,1,60,255,1,'',NULL,0),
(47210025,645516,0,0,0,'0',0,0,0,-1,-8403.77,-3713.76,16.16,0,0,0,0,1,60,255,1,'',NULL,0),
(47210026,645516,0,0,0,'0',0,0,0,-1,-8556.05,-3883.28,12.74,0,0,0,0,1,60,255,1,'',NULL,0),
(47210027,648590,0,0,0,'0',0,0,0,-1,-8833.73,-3350.90,108.46,0,0,0,0,1,60,255,1,'',NULL,0),
(47210028,649232,0,0,0,'0',0,0,0,-1,-8311.06,-4905.68,25.60,0,0,0,0,1,60,255,1,'',NULL,0),
(47210029,649232,0,0,0,'0',0,0,0,-1,-8375.71,-4769.03,27.55,0,0,0,0,1,60,255,1,'',NULL,0),
(47210030,649232,0,0,0,'0',0,0,0,-1,-8374.75,-4909.46,25.41,0,0,0,0,1,60,255,1,'',NULL,0),
(47210031,649232,0,0,0,'0',0,0,0,-1,-8429.38,-4882.09,28.16,0,0,0,0,1,60,255,1,'',NULL,0),
(47210032,649232,0,0,0,'0',0,0,0,-1,-8433.91,-4845.18,28.81,0,0,0,0,1,60,255,1,'',NULL,0),
(47210033,649232,0,0,0,'0',0,0,0,-1,-8387.03,-4826.44,28.84,0,0,0,0,1,60,255,1,'',NULL,0),
(47210034,649232,0,0,0,'0',0,0,0,-1,-8330.78,-4844.57,28.52,0,0,0,0,1,60,255,1,'',NULL,0),
(47210035,649232,0,0,0,'0',0,0,0,-1,-8359.87,-4802.47,27.43,0,0,0,0,1,60,255,1,'',NULL,0);

-- C. Quest item drops (quest-only)
DELETE FROM `gameobject_loot_template` WHERE `Entry`=623246 AND `ItemType`=0 AND `Item` IN (266424);
INSERT INTO `gameobject_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES (623246,0,266424,100,1,1,0,1,1,'Twilight Page - quest 94879 (object, 1 per loot)');
DELETE FROM `gameobject_loot_template` WHERE `Entry`=138950 AND `ItemType`=0 AND `Item` IN (266884);
INSERT INTO `gameobject_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES (138950,0,266884,100,1,1,0,1,1,'Hollow Clam Shell - quest 94923 (object, 1 per loot)');
DELETE FROM `gameobject_loot_template` WHERE `Entry`=138957 AND `ItemType`=0 AND `Item` IN (266897);
INSERT INTO `gameobject_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES (138957,0,266897,100,1,1,0,1,1,'Stolen Tools - quest 94931 (object, 1 per loot)');
DELETE FROM `gameobject_loot_template` WHERE `Entry`=139130 AND `ItemType`=0 AND `Item` IN (266156);
INSERT INTO `gameobject_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES (139130,0,266156,100,1,1,0,1,1,'Fresh Manure - quest 94842 (object, 1 per loot)');
DELETE FROM `gameobject_loot_template` WHERE `Entry`=139561 AND `ItemType`=0 AND `Item` IN (268536);
INSERT INTO `gameobject_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES (139561,0,268536,100,1,1,0,1,1,'Hay Bale - quest 95156 (object, 1 per loot)');
DELETE FROM `gameobject_loot_template` WHERE `Entry`=140443 AND `ItemType`=0 AND `Item` IN (271431);
INSERT INTO `gameobject_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES (140443,0,271431,100,1,1,0,1,1,'Fresh Apple - quest 95561 (object, 1 per loot)');
DELETE FROM `gameobject_loot_template` WHERE `Entry`=140475 AND `ItemType`=0 AND `Item` IN (271620);
INSERT INTO `gameobject_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES (140475,0,271620,100,1,1,0,1,1,'Foreman''s Crate - quest 95943 (object, 1 per loot)');
DELETE FROM `gameobject_loot_template` WHERE `Entry`=140476 AND `ItemType`=0 AND `Item` IN (271877);
INSERT INTO `gameobject_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES (140476,0,271877,100,1,1,0,1,1,'Bundle of Wood - quest 96009 (object, 1 per loot)');
DELETE FROM `gameobject_loot_template` WHERE `Entry`=140524 AND `ItemType`=0 AND `Item` IN (271611);
INSERT INTO `gameobject_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES (140524,0,271611,100,1,1,0,1,1,'Cubic Draconia - quest 95934 (object, 1 per loot)');
DELETE FROM `gameobject_loot_template` WHERE `Entry`=140588 AND `ItemType`=0 AND `Item` IN (272004);
INSERT INTO `gameobject_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES (140588,0,272004,100,1,1,0,1,1,'Marsh Gas Vent - quest 96063 (object, 1 per loot)');
UPDATE `creature_template_difficulty` SET `LootID`=254560 WHERE `Entry`=254560 AND `LootID`=0;
DELETE FROM `creature_loot_template` WHERE `Entry`=254560 AND `ItemType`=0 AND `Item` IN (266425,267073);
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES (254560,0,266425,36,1,1,0,1,1,'Twilight Shadowmancer - quest 94881 (seen 5 of 14 kills)'),(254560,0,267073,25,1,1,0,1,1,'Twilight Shadowmancer - quest 94961 (seen 3 of 14 kills)');
UPDATE `creature_template_difficulty` SET `LootID`=255898 WHERE `Entry`=255898 AND `LootID`=0;
DELETE FROM `creature_loot_template` WHERE `Entry`=255898 AND `ItemType`=0 AND `Item` IN (268344);
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES (255898,0,268344,33,1,1,0,1,1,'Murloc Oracle - quest 95121 (seen 4 of 12 kills)');
UPDATE `creature_template_difficulty` SET `LootID`=255899 WHERE `Entry`=255899 AND `LootID`=0;
DELETE FROM `creature_loot_template` WHERE `Entry`=255899 AND `ItemType`=0 AND `Item` IN (267266);
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES (255899,0,267266,35,1,1,0,1,1,'Murloc Tideskimmer - quest 94842 (cache lists it as this NPCs quest item, INFERRED)');
UPDATE `creature_template_difficulty` SET `LootID`=255901 WHERE `Entry`=255901 AND `LootID`=0;
DELETE FROM `creature_loot_template` WHERE `Entry`=255901 AND `ItemType`=0 AND `Item` IN (268345);
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES (255901,0,268345,40,1,1,0,1,1,'Sand Crawler - quest 95121 (seen 6 of 15 kills)');
UPDATE `creature_template_difficulty` SET `LootID`=256798 WHERE `Entry`=256798 AND `LootID`=0;
DELETE FROM `creature_loot_template` WHERE `Entry`=256798 AND `ItemType`=0 AND `Item` IN (266425,267073);
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES (256798,0,266425,39,1,1,0,1,1,'Twilight Neophyte - quest 94881 (seen 9 of 23 kills)'),(256798,0,267073,25,1,1,0,1,1,'Twilight Neophyte - quest 94961 (seen 5 of 21 kills)');
UPDATE `creature_template_difficulty` SET `LootID`=256801 WHERE `Entry`=256801 AND `LootID`=0;
DELETE FROM `creature_loot_template` WHERE `Entry`=256801 AND `ItemType`=0 AND `Item` IN (268344);
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES (256801,0,268344,45,1,1,0,1,1,'Murloc Warrior - quest 95121 (seen 5 of 11 kills)');
UPDATE `creature_template_difficulty` SET `LootID`=256807 WHERE `Entry`=256807 AND `LootID`=0;
DELETE FROM `creature_loot_template` WHERE `Entry`=256807 AND `ItemType`=0 AND `Item` IN (268345);
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES (256807,0,268345,35,1,1,0,1,1,'Shore Crawler - quest 95121 (cache lists it as this NPCs quest item, INFERRED)');
UPDATE `creature_template_difficulty` SET `LootID`=256952 WHERE `Entry`=256952 AND `LootID`=0;
DELETE FROM `creature_loot_template` WHERE `Entry`=256952 AND `ItemType`=0 AND `Item` IN (267073);
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES (256952,0,267073,25,1,1,0,1,1,'Twilight Raider - quest 94961 (seen 2 of 21 kills)');
UPDATE `creature_template_difficulty` SET `LootID`=257001 WHERE `Entry`=257001 AND `LootID`=0;
DELETE FROM `creature_loot_template` WHERE `Entry`=257001 AND `ItemType`=0 AND `Item` IN (268341);
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES (257001,0,268341,100,1,1,0,1,1,'Old Rot-Chum - quest 95120 (the quest is about this NPC, one needed)');
UPDATE `creature_template_difficulty` SET `LootID`=258522 WHERE `Entry`=258522 AND `LootID`=0;
DELETE FROM `creature_loot_template` WHERE `Entry`=258522 AND `ItemType`=0 AND `Item` IN (265738);
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES (258522,0,265738,100,1,1,0,1,1,'Sootfur - quest 94617 (one needed, dropped every time (1 of 1))');
UPDATE `creature_template_difficulty` SET `LootID`=258573 WHERE `Entry`=258573 AND `LootID`=0;
DELETE FROM `creature_loot_template` WHERE `Entry`=258573 AND `ItemType`=0 AND `Item` IN (267266);
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES (258573,0,267266,25,1,1,0,1,1,'Mudfin Oracle - quest 94842 (seen 6 of 32 kills)');
UPDATE `creature_template_difficulty` SET `LootID`=258574 WHERE `Entry`=258574 AND `LootID`=0;
DELETE FROM `creature_loot_template` WHERE `Entry`=258574 AND `ItemType`=0 AND `Item` IN (267266);
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES (258574,0,267266,25,1,1,0,1,1,'Mudfin Warrior - quest 94842 (seen 2 of 23 kills)');
UPDATE `creature_template_difficulty` SET `LootID`=258588 WHERE `Entry`=258588 AND `LootID`=0;
DELETE FROM `creature_loot_template` WHERE `Entry`=258588 AND `ItemType`=0 AND `Item` IN (266157);
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES (258588,0,266157,25,1,1,0,1,1,'Jumbo Crawfish - quest 94842 (seen 4 of 17 kills)');
UPDATE `creature_template_difficulty` SET `LootID`=259267 WHERE `Entry`=259267 AND `LootID`=0;
DELETE FROM `creature_loot_template` WHERE `Entry`=259267 AND `ItemType`=0 AND `Item` IN (271432);
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES (259267,0,271432,25,1,1,0,1,1,'Plains Coyote - quest 95913 (seen 2 of 8 kills)');
UPDATE `creature_template_difficulty` SET `LootID`=260235 WHERE `Entry`=260235 AND `LootID`=0;
DELETE FROM `creature_loot_template` WHERE `Entry`=260235 AND `ItemType`=0 AND `Item` IN (268586);
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES (260235,0,268586,100,1,1,0,1,1,'Trapclaw - quest 95254 (the quest is about this NPC, one needed)');
UPDATE `creature_template_difficulty` SET `LootID`=263348 WHERE `Entry`=263348 AND `LootID`=0;
DELETE FROM `creature_loot_template` WHERE `Entry`=263348 AND `ItemType`=0 AND `Item` IN (271778);
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES (263348,0,271778,35,1,1,0,1,1,'Roving Tallstrider - quest 95984 (seen 1 of 2 kills, INFERRED)');

-- D. Quests (38)
-- 94613 My Kingdom for a Horse (level 38)  -- item rewards not seen, none given; races/flags as the zones cache quests, rep 2826 INFERRED
DELETE FROM `quest_template` WHERE `ID`=94613;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (94613,2,16591,0,0,0,1,1.0,0,1,0,36700168,0,6130900294268439629,'My Kingdom for a Horse','Go to Wheeler''s Grange to inquire about the missing horses.','Our warhorses come from two nearby farms. One of them, owned by the Wheeler family off the road just outside of town, has apparently lost all of their stock. The cause of this is currently unknown.$B$BWith no horses, we have no cavalry, and without cavalry, our campaign against the Horde stands no chance.$B$BThus, you see, this situation must be resolved with utmost haste. Of where to start, however, I am less sure.$B$BGo to Wheeler''s Grange and speak to Benjy. See what he has to say for himself.','','',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2826,3,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=94613;
DELETE FROM `quest_template_addon` WHERE `ID`=94613;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (94613,0);
DELETE FROM `quest_request_items` WHERE `ID`=94613;
DELETE FROM `quest_offer_reward` WHERE `ID`=94613;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (94613,'Big man himself sent you, huh? You come to help out, or just to hound me for horses like the last fella?',0);
DELETE FROM `quest_classic_money` WHERE `ID`=94613;
DELETE FROM `quest_classic_level` WHERE `ID`=94613;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (94613,38,35,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=94613;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (94613,2,38);
DELETE FROM `creature_queststarter` WHERE `quest`=94613;
DELETE FROM `gameobject_queststarter` WHERE `quest`=94613;
DELETE FROM `creature_questender` WHERE `quest`=94613;
DELETE FROM `gameobject_questender` WHERE `quest`=94613;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (257558,94613,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (258317,94613,0);

-- 94614 Freshwater Fiends (level 38)  -- item rewards not seen, none given; races/flags as the zones cache quests, rep 2826 INFERRED
DELETE FROM `quest_template` WHERE `ID`=94614;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (94614,2,16591,0,0,0,5,1.0,0,1,0,36700168,0,6130900294268439629,'Freshwater Fiends','Kill 8 Mudfin Oracles and 8 Mudfin Warriors along the river near Wheeler''s Grange.','You seen those murlocs down by the river, just west of here? Nasty things. Bad for my animals, and bad for the wildlife. Not to mention, they''ve settled right on top of my favorite fishing spot!$B$BThere ain''t a soul in the Riverglades who wouldn''t appreciate you getting rid of a few, that''s for sure. $B$BPlease, kill any you see. You''d be doin'' us a whole heap of good.','','',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2826,3,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=94614;
DELETE FROM `quest_objectives` WHERE `ID` IN (9900000,9900001);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (9900000,94614,0,0,0,258574,8,0,0,0,0,0,1,NULL,0),(9900001,94614,0,1,1,258573,8,0,0,0,0,0,1,NULL,0);
DELETE FROM `quest_template_addon` WHERE `ID`=94614;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (94614,0);
DELETE FROM `quest_request_items` WHERE `ID`=94614;
DELETE FROM `quest_offer_reward` WHERE `ID`=94614;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (94614,'Pest control''s an important part of any farmer''s job. I appreciate your help.',0);
DELETE FROM `quest_classic_money` WHERE `ID`=94614;
INSERT INTO `quest_classic_money` (`ID`,`RewardMoney`,`RewardMoneyMaxLevel`) VALUES (94614,2000,0);
DELETE FROM `quest_classic_level` WHERE `ID`=94614;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (94614,38,35,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=94614;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (94614,2,38);
DELETE FROM `creature_queststarter` WHERE `quest`=94614;
DELETE FROM `gameobject_queststarter` WHERE `quest`=94614;
DELETE FROM `creature_questender` WHERE `quest`=94614;
DELETE FROM `gameobject_questender` WHERE `quest`=94614;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (258317,94614,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (258317,94614,0);

-- 94617 Coyote Huntin' (level 38)  -- item rewards not seen, none given; races/flags as the zones cache quests, rep 2826 INFERRED
DELETE FROM `quest_template` WHERE `ID`=94617;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (94617,2,16591,0,0,0,5,1.0,0,1,0,36700168,0,6130900294268439629,'Coyote Huntin''','Kill the coyote known as Sootfur, and bring its pelt back to Benjy Wheeler.','There''s a pack of coyotes livin'' in a den west of here. We can see ''em sometimes from across the river. They used to hunt fish on the shore, but I guess the murlocs pushed ''em out of their usual feeding ground, and so they''ve taken to eating horses.$B$BThe leader of the coyote pack''s a bigger one with dark fur. We''d nicknamed him "Sootfur". I reckon if you kill him, the rest''ll scatter.','','',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2826,3,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=94617;
DELETE FROM `quest_objectives` WHERE `ID` IN (9900002);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (9900002,94617,1,0,0,265738,1,0,0,0,0,0,1,NULL,0);
DELETE FROM `quest_template_addon` WHERE `ID`=94617;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (94617,0);
DELETE FROM `quest_request_items` WHERE `ID`=94617;
INSERT INTO `quest_request_items` (`ID`,`CompletionText`,`VerifiedBuild`) VALUES (94617,'I''m gonna make a hat out of that varmint, I''ll tell you what!',0);
DELETE FROM `quest_offer_reward` WHERE `ID`=94617;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (94617,'Thank you, $n.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=94617;
DELETE FROM `quest_classic_level` WHERE `ID`=94617;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (94617,38,35,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=94617;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (94617,2,38);
DELETE FROM `creature_queststarter` WHERE `quest`=94617;
DELETE FROM `gameobject_queststarter` WHERE `quest`=94617;
DELETE FROM `creature_questender` WHERE `quest`=94617;
DELETE FROM `gameobject_questender` WHERE `quest`=94617;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (258317,94617,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (258317,94617,0);

-- 94678 Gnoll Infestation (level 38)
DELETE FROM `quest_template` WHERE `ID`=94678;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (94678,2,16591,0,0,0,5,1.0,0,1,0,36700168,0,6130900294268439629,'Gnoll Infestation','Kill $1oa Redridge Mystics, $2oa Redridge Trappers, and $3oa Redridge Brutes for Cedric Dalton at Southern Watch.','The gnolls have been a thorn in our side ever since we arrived. While less a threat than the Twilight''s Hammer, they are aggressive, persistent and, worst of all, numerous.$B$BThe hills beyond this post are infested with them, making passage to the north dangerous, and posing a threat to those living outside the walls of Farholde Keep.$B$BIf we are ever to make these lands safe, removing the gnoll threat is imperative.','','',70862,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2826,5,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=94678;
DELETE FROM `quest_objectives` WHERE `ID` IN (469074,469075,469076);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (469074,94678,0,0,0,255770,8,0,32,0,0.0,0,1,NULL,70862),(469075,94678,0,1,1,255772,8,0,32,0,0.0,0,1,NULL,70862),(469076,94678,0,2,2,255769,6,0,32,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_template_addon` WHERE `ID`=94678;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (94678,0);
DELETE FROM `quest_request_items` WHERE `ID`=94678;
DELETE FROM `quest_offer_reward` WHERE `ID`=94678;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (94678,'Is that gnoll spittle on you? No matter. Under any other circumstance I''d question where that came from, but this--$B$B<Cedric motions up and down your frame with his hand.>$B$BThis is what we do out here.',0);
DELETE FROM `quest_classic_money` WHERE `ID`=94678;
DELETE FROM `quest_classic_level` WHERE `ID`=94678;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (94678,38,36,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=94678;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (94678,2,38);
DELETE FROM `creature_queststarter` WHERE `quest`=94678;
DELETE FROM `gameobject_queststarter` WHERE `quest`=94678;
DELETE FROM `creature_questender` WHERE `quest`=94678;
DELETE FROM `gameobject_questender` WHERE `quest`=94678;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (259353,94678,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (259353,94678,0);

-- 94841 Lost Shipping Crate (level 38)
DELETE FROM `quest_template` WHERE `ID`=94841;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (94841,2,405,0,0,0,1,1.0,0,1,276067,36700416,0,6130900294268439629,'Lost Shipping Crate','Deliver the Lost Shipping Crate to Friz Frazzlespark at Powderfuse Port in the Riverglades.','<A handwritten label appears on the side of the crate. On the right side is a map with a large red circle along a southeastern shoreline north of a poorly drawn temple surrounded by water and some kind of winged creatures. On the left is a note in massive font.>$B$BPROPERTY OF FRIZ FRAZZLESPARK$B$BPOWDERFUSE PORT$B$BWARNING: NOT for tampering. Do NOT open without supervision.$B$B<An additional note appears at the bottom of the label.>$B$BIf lost, return to Friz Frazzlespark. Powderfuse Port. Riverglades.','','',70862,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2827,1,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=94841;
DELETE FROM `quest_template_addon` WHERE `ID`=94841;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (94841,0);
DELETE FROM `quest_request_items` WHERE `ID`=94841;
DELETE FROM `quest_offer_reward` WHERE `ID`=94841;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (94841,'Where did you say you found this? I''m surprised you''re still fully intact. Thanks, kid.$B$BY''know. If you''re in the habit of doin'' favors, I bet plenty of folk ''round here will have a use for ya. Why don''tcha stick around for a bit?',0);
DELETE FROM `quest_classic_money` WHERE `ID`=94841;
DELETE FROM `quest_classic_level` WHERE `ID`=94841;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (94841,38,36,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=94841;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (94841,2,38);
DELETE FROM `creature_queststarter` WHERE `quest`=94841;
DELETE FROM `gameobject_queststarter` WHERE `quest`=94841;
DELETE FROM `creature_questender` WHERE `quest`=94841;
DELETE FROM `gameobject_questender` WHERE `quest`=94841;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (255897,94841,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (255897,94841,0);

-- 94842 Unsavory Plant Food (level 38)
DELETE FROM `quest_template` WHERE `ID`=94842;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (94842,2,16591,0,0,0,5,1.0,0,1,0,3145736,0,6130900294268439629,'Unsavory Plant Food','Gather 4 Jumbo Crawfish Shells, 4 pieces of Fresh Manure, and 8 Mudfin Murloc Heads for Halligan Turner.','We make our own fertilizer out here. It''s a special formula of mine that reeks like hell, but grows trees like they''re weeds.$B$BIt''s a 1:1:2 ratio of crawfish shells, manure, and murloc heads. Tasty, right?$B$BThe manure''s easy enough. We''ve got the ranch to the south for that. The heads and shells? Those you''re going to have to go wading in the river for.$B$BIf you feel like lending a hand, we could sure use your help getting some fertilizer ingredients.','','',70862,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2826,5,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=94842;
DELETE FROM `quest_objectives` WHERE `ID` IN (469226,469225,469578);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (469226,94842,1,0,2,266157,4,0,32,0,0.0,0,1,NULL,70862),(469225,94842,1,1,0,266156,4,0,32,0,0.0,0,1,NULL,70862),(469578,94842,1,2,1,267266,8,0,32,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_template_addon` WHERE `ID`=94842;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (94842,0);
DELETE FROM `quest_request_items` WHERE `ID`=94842;
INSERT INTO `quest_request_items` (`ID`,`CompletionText`,`VerifiedBuild`) VALUES (94842,'Some folks think lumber work''s a simpleton''s job. They''d be dead wrong. Having skills like botany and chemistry can get you far in this business.',0);
DELETE FROM `quest_offer_reward` WHERE `ID`=94842;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (94842,'Thank you, $n.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=94842;
INSERT INTO `quest_classic_money` (`ID`,`RewardMoney`,`RewardMoneyMaxLevel`) VALUES (94842,2000,0);
DELETE FROM `quest_classic_level` WHERE `ID`=94842;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (94842,38,36,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=94842;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (94842,2,38);
DELETE FROM `creature_queststarter` WHERE `quest`=94842;
DELETE FROM `gameobject_queststarter` WHERE `quest`=94842;
DELETE FROM `creature_questender` WHERE `quest`=94842;
DELETE FROM `gameobject_questender` WHERE `quest`=94842;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (256438,94842,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (256438,94842,0);

-- 94877 Saving Sunnyglade (level 40)
DELETE FROM `quest_template` WHERE `ID`=94877;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (94877,2,16591,0,0,0,5,1.0,0,1,0,36700168,0,6130900294268439629,'Saving Sunnyglade','Defeat $1oa Twilight Raiders, $2oa Twilight Shadowmancers, and $3oa Twilight Neophytes within Sunnyglade for Demar Brant.','When you look around you, what do you see? A broken home and a tired people.$B$BStormwind has left us to fend for ourselves, and we have less to do that with every day. Many within these walls are refugees who barely escaped tragedy without so much as even their own families.$B$BFarholde is no Stormwind, stranger. We lost a settlement in the northwest to the Twilight''s Hammer, but we will not let them keep it. If you wish to fight, go to Sunnyglade and purge those savages from our lands.','','',70862,0,0,0,0,0,0,0,0,274919,1,0,274920,1,0,0,0,0,0,0,0,0,0,0,0,0,0,2826,5,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=94877;
DELETE FROM `quest_objectives` WHERE `ID` IN (469290,469291,469292);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (469290,94877,0,0,2,256952,8,0,32,0,0.0,0,1,NULL,70862),(469291,94877,0,1,3,254560,8,0,32,0,0.0,0,1,NULL,70862),(469292,94877,0,2,4,256798,8,0,32,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_template_addon` WHERE `ID`=94877;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (94877,0);
DELETE FROM `quest_request_items` WHERE `ID`=94877;
INSERT INTO `quest_request_items` (`ID`,`CompletionText`,`VerifiedBuild`) VALUES (94877,'We''ve held out for this long, and we won''t back down now. Show those Twilight cultists the strength of human tenacity!',0);
DELETE FROM `quest_offer_reward` WHERE `ID`=94877;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (94877,'You do your Alliance proud, gnome. If the Stormwind aristocracy had even half your mettle, we''d have ended this war years ago.',0);
DELETE FROM `quest_classic_money` WHERE `ID`=94877;
DELETE FROM `quest_classic_level` WHERE `ID`=94877;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (94877,40,36,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=94877;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (94877,2,40);
DELETE FROM `creature_queststarter` WHERE `quest`=94877;
DELETE FROM `gameobject_queststarter` WHERE `quest`=94877;
DELETE FROM `creature_questender` WHERE `quest`=94877;
DELETE FROM `gameobject_questender` WHERE `quest`=94877;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (257558,94877,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (257558,94877,0);

-- 94879 Scraps of Knowledge (level 40)
DELETE FROM `quest_template` WHERE `ID`=94879;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (94879,2,16591,0,0,0,5,1.0,0,1,0,36700168,0,6130900294268439629,'Scraps of Knowledge','Collect $1oa Twilight Pages from within Sunnyglade for Ashe Amberall in Farholde Keep.','You''ve seen the ruined city to the north, yes? Sunnyglade? Demar tries sending troops, but we''re no closer to reclaiming it. I have a better idea.$B$BIn truth, this attack by the Twilight''s Hammer is not the first. They seem to be coordinating from a centralized location, and I would know where. While you''re out doing what you adventurers do best, see if you can''t keep an eye out for any documents they may have misplaced. With enough of them, I should be able to piece together their intentions.','','',70862,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2826,5,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=94879;
DELETE FROM `quest_objectives` WHERE `ID` IN (469294);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (469294,94879,1,0,2,266424,5,1434,32,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_template_addon` WHERE `ID`=94879;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (94879,0);
DELETE FROM `quest_request_items` WHERE `ID`=94879;
INSERT INTO `quest_request_items` (`ID`,`CompletionText`,`VerifiedBuild`) VALUES (94879,'The Twilight''s Hammer are dogmatic, but hardly organized. You''re likely to find their pages scattered throughout Sunnyglade.',0);
DELETE FROM `quest_offer_reward` WHERE `ID`=94879;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (94879,'Thank you, $n.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=94879;
DELETE FROM `quest_classic_level` WHERE `ID`=94879;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (94879,40,36,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=94879;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (94879,2,40);
DELETE FROM `creature_queststarter` WHERE `quest`=94879;
DELETE FROM `gameobject_queststarter` WHERE `quest`=94879;
DELETE FROM `creature_questender` WHERE `quest`=94879;
DELETE FROM `gameobject_questender` WHERE `quest`=94879;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (257591,94879,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (257591,94879,0);

-- 94881 Curious Trinkets (level 38)
DELETE FROM `quest_template` WHERE `ID`=94881;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (94881,2,16591,0,0,0,5,1.0,0,1,0,36700168,0,6130900294268439629,'Curious Trinkets','Retrieve $1oa Twilight Charms from Twilight Shadowmancers and Twilight Neophytes in Sunnyglade for Ashe Amberhall in Farholde Keep.','My, what have we here? A brave new $c come to save us from our woes? How charming! Maybe you can help me with something then? I''d quite like to learn more about this cult in Sunnyglade.$B$BLike any cult, they operate in a caste system. Those with an affinity for magic are often in high favor and are given tokens to demonstrate such. These charms are then imbued, and thus have a rich abundance of cultist magic.$B$BI wish to see these tokens for myself. Be a dear and see if you can bring me a few, hmm?','','',70862,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2826,5,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=94881;
DELETE FROM `quest_objectives` WHERE `ID` IN (469295);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (469295,94881,1,0,0,266425,25,1434,32,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_template_addon` WHERE `ID`=94881;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (94881,0);
DELETE FROM `quest_request_items` WHERE `ID`=94881;
INSERT INTO `quest_request_items` (`ID`,`CompletionText`,`VerifiedBuild`) VALUES (94881,'You shouldn''t have too much trouble finding charms off of Twilight''s Hammer spellcasters. They seem to hoard them quite jealously.',0);
DELETE FROM `quest_offer_reward` WHERE `ID`=94881;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (94881,'Thank you, $n.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=94881;
DELETE FROM `quest_classic_level` WHERE `ID`=94881;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (94881,38,36,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=94881;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (94881,2,38);
DELETE FROM `creature_queststarter` WHERE `quest`=94881;
DELETE FROM `gameobject_queststarter` WHERE `quest`=94881;
DELETE FROM `creature_questender` WHERE `quest`=94881;
DELETE FROM `gameobject_questender` WHERE `quest`=94881;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (257591,94881,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (257591,94881,0);

-- 94886 The Western Front (level 44)
DELETE FROM `quest_template` WHERE `ID`=94886;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (94886,2,16591,0,0,0,1,1.0,0,1,0,36700168,0,6130900294268439629,'The Western Front','Find Sergeant Riftan near the Twilight''s Shroud to the west.','As helpful as you''ve been here in Farholde, I believe your talents could be put to better use elsewhere.$B$BA patrol of ours went missing some time ago. They were in charge of our western borders, but with the information Miss Amberhall has uncovered, I fear they walked right into the Twilight Hammer''s central encampment.$B$BWe can handle the cleanup in Sunnyglade ourselves. If the good lady is right, it''s critical that we check on our wayward sergeant. He could use all the help he can get.','','Find Sergeant Riftan near the Twilight''s Shroud to the west.',70862,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2826,1,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=94886;
DELETE FROM `quest_template_addon` WHERE `ID`=94886;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (94886,0);
DELETE FROM `quest_request_items` WHERE `ID`=94886;
DELETE FROM `quest_offer_reward` WHERE `ID`=94886;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (94886,'If you''re here, Amberhall must be nagging the Knight Commander about our lack of reports. Don''t let her sweet talk fool you, she''d sell your soul if she thought it was worth something.',0);
DELETE FROM `quest_classic_money` WHERE `ID`=94886;
DELETE FROM `quest_classic_level` WHERE `ID`=94886;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (94886,44,38,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=94886;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (94886,2,44);
DELETE FROM `creature_queststarter` WHERE `quest`=94886;
DELETE FROM `gameobject_queststarter` WHERE `quest`=94886;
DELETE FROM `creature_questender` WHERE `quest`=94886;
DELETE FROM `gameobject_questender` WHERE `quest`=94886;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (257558,94886,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (258023,94886,0);

-- 94923 Hats Off (level 38)
DELETE FROM `quest_template` WHERE `ID`=94923;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (94923,2,16591,0,0,0,4,1.0,0,1,0,3145736,0,6130900294268439629,'Hats Off','Find Robbie''s Hat somewhere along the river near the murlocs.','<Robbie leans in to talk to you, as if sharing a grave secret.>$B$BPsst! Hey $gmister:miss;, I need your help!$B$BMy sister got me a hat for my birthday but I lost it playing in the river. I''m not allowed to be down there because of the murlocs so I can''t ask Dad or Ellie for help because then they''ll know where I was!$B$BIt''s a good hat. I bet one of those murlocs found it and kept it for himself!','','',70862,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2826,5,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=94923;
DELETE FROM `quest_objectives` WHERE `ID` IN (469335);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (469335,94923,1,0,0,266884,1,1769107308,32,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_template_addon` WHERE `ID`=94923;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (94923,0);
DELETE FROM `quest_request_items` WHERE `ID`=94923;
INSERT INTO `quest_request_items` (`ID`,`CompletionText`,`VerifiedBuild`) VALUES (94923,'Ooh... Ellie''s gonna be so mad if she finds out I lost that hat.',0);
DELETE FROM `quest_offer_reward` WHERE `ID`=94923;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (94923,'Thank you, $n.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=94923;
INSERT INTO `quest_classic_money` (`ID`,`RewardMoney`,`RewardMoneyMaxLevel`) VALUES (94923,2000,0);
DELETE FROM `quest_classic_level` WHERE `ID`=94923;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (94923,38,36,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=94923;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (94923,2,38);
DELETE FROM `creature_queststarter` WHERE `quest`=94923;
DELETE FROM `gameobject_queststarter` WHERE `quest`=94923;
DELETE FROM `creature_questender` WHERE `quest`=94923;
DELETE FROM `gameobject_questender` WHERE `quest`=94923;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (259049,94923,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (259049,94923,0);

-- 94924 Hunting the Hunters (level 39)  -- item rewards not seen, none given; races/flags as the zones cache quests, rep 2826 INFERRED
DELETE FROM `quest_template` WHERE `ID`=94924;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (94924,2,16591,0,0,0,5,1.0,0,1,0,36700168,0,6130900294268439629,'Hunting the Hunters','Kill 12 Rog''mar Poachers for Watcher Cornelius.','The orcish poachers are a bloodthirsty lot. They''ll kill anything that moves--be it men, lions, or horses. They are a threat to all life in the Riverglades, and must be stopped.$B$BThey roam the plains in packs, like rabid animals. If you see any, put them down.','','',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2826,3,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=94924;
DELETE FROM `quest_objectives` WHERE `ID` IN (9900003);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (9900003,94924,0,0,0,259052,12,0,0,0,0,0,1,NULL,0);
DELETE FROM `quest_template_addon` WHERE `ID`=94924;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (94924,0);
DELETE FROM `quest_request_items` WHERE `ID`=94924;
DELETE FROM `quest_offer_reward` WHERE `ID`=94924;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (94924,'Cut down quite a few of them, eh? Perhaps you''re not as soft as I first took you for.$B$BI hope the buzzards are making a meal of those savages.',0);
DELETE FROM `quest_classic_money` WHERE `ID`=94924;
INSERT INTO `quest_classic_money` (`ID`,`RewardMoney`,`RewardMoneyMaxLevel`) VALUES (94924,2500,0);
DELETE FROM `quest_classic_level` WHERE `ID`=94924;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (94924,39,36,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=94924;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (94924,2,39);
DELETE FROM `creature_queststarter` WHERE `quest`=94924;
DELETE FROM `gameobject_queststarter` WHERE `quest`=94924;
DELETE FROM `creature_questender` WHERE `quest`=94924;
DELETE FROM `gameobject_questender` WHERE `quest`=94924;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (259352,94924,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (259352,94924,0);

-- 94928 A Few Furs for Friz (level 39)
DELETE FROM `quest_template` WHERE `ID`=94928;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (94928,2,16591,0,0,95288,1,1.0,0,1,267263,36700160,0,6130900294268439629,'A Few Furs for Friz','Deliver the Bundle of Lion Hides to Friz Frazzlespark at Powderfuse Port.','These hides are to be delivered to Powderfuse Port. It is a goblin-run port to the northeast.$B$BWhen you get there, deliver the hides to Friz Frazzlespark. Tell her they''re from Cornelius. The details of our trade have already been worked out, so she''ll know what they are for.','','',70862,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2826,5,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=94928;
DELETE FROM `quest_objectives` WHERE `ID` IN (469342);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (469342,94928,1,0,1,267263,1,1434,32,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_template_addon` WHERE `ID`=94928;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (94928,0);
DELETE FROM `quest_request_items` WHERE `ID`=94928;
INSERT INTO `quest_request_items` (`ID`,`CompletionText`,`VerifiedBuild`) VALUES (94928,'Whaddya got for me?',0);
DELETE FROM `quest_offer_reward` WHERE `ID`=94928;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (94928,'Thank you, $n.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=94928;
DELETE FROM `quest_classic_level` WHERE `ID`=94928;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (94928,39,36,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=94928;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (94928,2,39);
DELETE FROM `creature_queststarter` WHERE `quest`=94928;
DELETE FROM `gameobject_queststarter` WHERE `quest`=94928;
DELETE FROM `creature_questender` WHERE `quest`=94928;
DELETE FROM `gameobject_questender` WHERE `quest`=94928;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (259352,94928,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (255897,94928,0);

-- 94931 Stolen Saws (level 38)
DELETE FROM `quest_template` WHERE `ID`=94931;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (94931,2,16591,0,0,0,5,1.0,0,1,0,3145736,0,6130900294268439629,'Stolen Saws','Recover $1oa Saw Blades from the gnoll camps in Bristle Hills for Halligan Turner at the lumber mill.','The boys up at Southern Watch do a good job keeping gnolls out for the most part. Every now and then a few sneak into the camp at night and cause a bit of trouble. They especially like to steal saw blades, which they likely use to make weapons out of the sharp metal.$B$BIf you could get some back for us, it''d be a big help. The gnolls have probably got ''em stashed away somewhere in their camps up in the hills to the west.','','',70862,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2826,5,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=94931;
DELETE FROM `quest_objectives` WHERE `ID` IN (469347);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (469347,94931,1,0,1,266897,8,1434,32,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_template_addon` WHERE `ID`=94931;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (94931,0);
DELETE FROM `quest_request_items` WHERE `ID`=94931;
INSERT INTO `quest_request_items` (`ID`,`CompletionText`,`VerifiedBuild`) VALUES (94931,'Those saw blades aren''t easy to come by. Any you can bring back would save us a great deal of time and resources.',0);
DELETE FROM `quest_offer_reward` WHERE `ID`=94931;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (94931,'Thank you, $n.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=94931;
DELETE FROM `quest_classic_level` WHERE `ID`=94931;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (94931,38,36,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=94931;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (94931,2,38);
DELETE FROM `creature_queststarter` WHERE `quest`=94931;
DELETE FROM `gameobject_queststarter` WHERE `quest`=94931;
DELETE FROM `creature_questender` WHERE `quest`=94931;
DELETE FROM `gameobject_questender` WHERE `quest`=94931;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (256438,94931,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (256438,94931,0);

-- 94961 Biting the Hand (level 40)
DELETE FROM `quest_template` WHERE `ID`=94961;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (94961,2,16591,0,0,94963,4,1.0,0,1,0,36700168,0,18446744073709551615,'Biting the Hand','Bring 20 Twilight Finger Bones to Fuzzle.','<The gnoll pulls a bit of gold from his pocket and waves it at you.>$B$B$R, I offer trade! There is more where this came from. Help me get revenge, and I will show you!','','',70862,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=94961;
DELETE FROM `quest_objectives` WHERE `ID` IN (469455);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (469455,94961,1,0,0,267073,20,1434,32,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_template_addon` WHERE `ID`=94961;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (94961,0);
DELETE FROM `quest_request_items` WHERE `ID`=94961;
INSERT INTO `quest_request_items` (`ID`,`CompletionText`,`VerifiedBuild`) VALUES (94961,'Bring me fingers, *then* I show you treasure.',0);
DELETE FROM `quest_offer_reward` WHERE `ID`=94961;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (94961,'Thank you, $n.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=94961;
DELETE FROM `quest_classic_level` WHERE `ID`=94961;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (94961,40,36,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=94961;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (94961,2,40);
DELETE FROM `creature_queststarter` WHERE `quest`=94961;
DELETE FROM `gameobject_queststarter` WHERE `quest`=94961;
DELETE FROM `creature_questender` WHERE `quest`=94961;
DELETE FROM `gameobject_questender` WHERE `quest`=94961;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (259158,94961,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (259158,94961,0);

-- 94999 Eyes for the Watchman (level 36)
DELETE FROM `quest_template` WHERE `ID`=94999;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (94999,2,16591,0,0,0,2,1.0,0,1,267244,3145728,0,6130900294268439629,'Eyes for the Watchman','Deliver Eleanor''s care package to Watcher Fredericks at Southern Watch.','Oh! Sorry, I didn''t see you there.$B$BI was just putting together a little something for a friend of mine. Well... more of an acquaintance, really. Have you met Watcher Fredericks yet? He''s stationed at Southern Watch just west of here.$B$BHe comes by the ranch on patrol every now and then. I thought it would be nice to give him a little something to show my appreciation.$B$BI''d deliver it myself, but I need to keep an eye on my little brother and tend to the ranch.$B$BI''m sure you understand.','','',70862,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2826,5,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=94999;
DELETE FROM `quest_objectives` WHERE `ID` IN (469554);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (469554,94999,1,0,1,267244,1,7686,32,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_template_addon` WHERE `ID`=94999;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (94999,0);
DELETE FROM `quest_request_items` WHERE `ID`=94999;
INSERT INTO `quest_request_items` (`ID`,`CompletionText`,`VerifiedBuild`) VALUES (94999,'Something to report?',0);
DELETE FROM `quest_offer_reward` WHERE `ID`=94999;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (94999,'Thank you, $n.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=94999;
DELETE FROM `quest_classic_level` WHERE `ID`=94999;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (94999,36,36,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=94999;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (94999,2,36);
DELETE FROM `creature_queststarter` WHERE `quest`=94999;
DELETE FROM `gameobject_queststarter` WHERE `quest`=94999;
DELETE FROM `creature_questender` WHERE `quest`=94999;
DELETE FROM `gameobject_questender` WHERE `quest`=94999;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (259050,94999,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (259051,94999,0);

-- 95059 Frazzlespark's Crate (level 36)  -- item rewards not seen, none given; races/flags as the zones cache quests, rep 2826 INFERRED
DELETE FROM `quest_template` WHERE `ID`=95059;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (95059,2,16591,0,0,0,1,1.0,0,1,0,36700168,0,6130900294268439629,'Frazzlespark''s Crate','Deliver Frazzlespark''s Crate to Knight Commander Demar Brant in Farholde Keep.','Time for a special delivery.$B$BKnight Commander Demar Brant at Farholde is waiting for this. For some reason he always makes me say his full name. What an odd fella.$B$BAnywho, you can find him holed up in the keep southwest of here. Massive stone fortress. Ya can''t miss it.','','',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2826,3,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=95059;
DELETE FROM `quest_template_addon` WHERE `ID`=95059;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (95059,0);
DELETE FROM `quest_request_items` WHERE `ID`=95059;
DELETE FROM `quest_offer_reward` WHERE `ID`=95059;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (95059,'That goblin is nothing more than a means to an end. There are certain dealings that must be done to ensure the safety and success of our people.',0);
DELETE FROM `quest_classic_money` WHERE `ID`=95059;
DELETE FROM `quest_classic_level` WHERE `ID`=95059;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (95059,36,33,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=95059;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (95059,2,36);
DELETE FROM `creature_queststarter` WHERE `quest`=95059;
DELETE FROM `gameobject_queststarter` WHERE `quest`=95059;
DELETE FROM `creature_questender` WHERE `quest`=95059;
DELETE FROM `gameobject_questender` WHERE `quest`=95059;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (255897,95059,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (257558,95059,0);

-- 95064 Preventative Measures (level 39)  -- reward item(s) 274959 left out: no item data in our client files yet
DELETE FROM `quest_template` WHERE `ID`=95064;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (95064,2,16591,0,0,0,5,1.0,0,1,0,36700168,0,6130900294268439629,'Preventative Measures','Kill $1oa Rog''mar Trainees for Watcher Cornelius at Terral''s Watch.','The orcs have become an encroaching threat ever since their arrival. First, claiming the lands by the river and now their presence grows within the plains...$B$BI''ll never understand their kind, nor do I care, truth be told, but I''ll be damned if they think there''s more ground to be taken.','','',70862,0,0,0,0,0,0,0,0,274961,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2826,5,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=95064;
DELETE FROM `quest_objectives` WHERE `ID` IN (473740);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (473740,95064,0,0,0,256749,12,1434,32,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_template_addon` WHERE `ID`=95064;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (95064,0);
DELETE FROM `quest_request_items` WHERE `ID`=95064;
DELETE FROM `quest_offer_reward` WHERE `ID`=95064;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (95064,'Thank you, $n.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=95064;
DELETE FROM `quest_classic_level` WHERE `ID`=95064;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (95064,39,36,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=95064;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (95064,2,39);
DELETE FROM `creature_queststarter` WHERE `quest`=95064;
DELETE FROM `gameobject_queststarter` WHERE `quest`=95064;
DELETE FROM `creature_questender` WHERE `quest`=95064;
DELETE FROM `gameobject_questender` WHERE `quest`=95064;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (259352,95064,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (259352,95064,0);

-- 95118 Saltwater Savages (level 41)
DELETE FROM `quest_template` WHERE `ID`=95118;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (95118,2,16591,0,0,0,5,1.0,0,1,0,36700168,0,18446744073709551615,'Saltwater Savages','Kill 8 Murloc Oracles and 8 Murloc Warriors along the Eastwind Shore.','Murlocs, murlocs, murlocs! That''s all I ever hear about; from the sailors, the fishermen, even my own kids! Everyone''s on my case about ''em! They''re a nuisance, I admit, but I got a port to run here!$B$BHelp a guy out, would ya? Kill a couple ''a those things and I''ll make it worth your time.','','',70862,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2827,5,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=95118;
DELETE FROM `quest_objectives` WHERE `ID` IN (469848,469849);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (469848,95118,0,0,2,256801,8,1869374800,32,0,0.0,0,1,NULL,70862),(469849,95118,0,1,3,255898,8,0,32,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_template_addon` WHERE `ID`=95118;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (95118,0);
DELETE FROM `quest_request_items` WHERE `ID`=95118;
DELETE FROM `quest_offer_reward` WHERE `ID`=95118;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (95118,'You handled ''em? Good! I''m sure more''ll be back soon, but at least I can get some peace and quiet until then.',0);
DELETE FROM `quest_classic_money` WHERE `ID`=95118;
INSERT INTO `quest_classic_money` (`ID`,`RewardMoney`,`RewardMoneyMaxLevel`) VALUES (95118,4000,0);
DELETE FROM `quest_classic_level` WHERE `ID`=95118;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (95118,41,36,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=95118;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (95118,2,41);
DELETE FROM `creature_queststarter` WHERE `quest`=95118;
DELETE FROM `gameobject_queststarter` WHERE `quest`=95118;
DELETE FROM `creature_questender` WHERE `quest`=95118;
DELETE FROM `gameobject_questender` WHERE `quest`=95118;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (259961,95118,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (259961,95118,0);

-- 95120 WANTED: Old Rot-Chum (level 42)
DELETE FROM `quest_template` WHERE `ID`=95120;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (95120,2,16591,1,0,0,6,1.0,0,1,0,36700168,0,18446744073709551615,'WANTED: Old Rot-Chum','Bring proof of Old Rot-Chum''s demise to Wharfmaster Whizzbuzz.','Sailors and swimmers beware! An especially aggressive murloc, nicknamed "Old Rot-Chum" by local fishermen, has been seen attacking rowboats and other small vessels in the waters to the south, near a collection of murloc-infested islands.$B$BA reward is being offered by Wharfmaster Whizzbuzz in exchange for proof of the creature''s demise.','','',70862,274963,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2827,6,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=95120;
DELETE FROM `quest_objectives` WHERE `ID` IN (469852);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (469852,95120,1,0,0,268341,1,1434,32,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_template_addon` WHERE `ID`=95120;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (95120,0);
DELETE FROM `quest_request_items` WHERE `ID`=95120;
DELETE FROM `quest_offer_reward` WHERE `ID`=95120;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (95120,'Thank you, $n.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=95120;
INSERT INTO `quest_classic_money` (`ID`,`RewardMoney`,`RewardMoneyMaxLevel`) VALUES (95120,3000,0);
DELETE FROM `quest_classic_level` WHERE `ID`=95120;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (95120,42,36,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=95120;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (95120,2,42);
DELETE FROM `creature_queststarter` WHERE `quest`=95120;
DELETE FROM `gameobject_queststarter` WHERE `quest`=95120;
DELETE FROM `creature_questender` WHERE `quest`=95120;
DELETE FROM `gameobject_questender` WHERE `quest`=95120;
INSERT INTO `gameobject_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (628948,95120,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (259961,95120,0);

-- 95121 A Taste of the Sea (level 40)
DELETE FROM `quest_template` WHERE `ID`=95121;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (95121,2,16591,0,0,0,5,1.0,0,1,0,3145992,0,18446744073709551615,'A Taste of the Sea','Gather 12 Murloc Eyes and 6 pieces of Sand Crawler Meat for Fimbo Greasemitz.','Welcome to Fimbo''s cafe on the water! Today''s menu is roasted boar, and roasted fish...same as every day...$B$BThis menu needs a shake up, badly. I''ve had my eye on a new seafood stew recipe, but I''m gonna need some fresh meat. A few murloc eyes and some crab meat oughta do.$B$BYou can find both along the beaches nearby. Can''t beat some good ol'' fashioned local ingredients!','','',70862,274972,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2827,5,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=95121;
DELETE FROM `quest_objectives` WHERE `ID` IN (469859,469860);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (469859,95121,1,0,3,268344,12,1869374800,32,0,0.0,0,1,NULL,70862),(469860,95121,1,1,4,268345,6,0,32,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_template_addon` WHERE `ID`=95121;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (95121,0);
DELETE FROM `quest_request_items` WHERE `ID`=95121;
INSERT INTO `quest_request_items` (`ID`,`CompletionText`,`VerifiedBuild`) VALUES (95121,'The trick to murloc eyes is you gotta cook ''em real quick. That way, you can still taste the brine!',0);
DELETE FROM `quest_offer_reward` WHERE `ID`=95121;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (95121,'Thank you, $n.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=95121;
DELETE FROM `quest_classic_level` WHERE `ID`=95121;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (95121,40,36,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=95121;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (95121,2,40);
DELETE FROM `creature_queststarter` WHERE `quest`=95121;
DELETE FROM `gameobject_queststarter` WHERE `quest`=95121;
DELETE FROM `creature_questender` WHERE `quest`=95121;
DELETE FROM `gameobject_questender` WHERE `quest`=95121;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (259962,95121,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (259962,95121,0);

-- 95147 The Lost Scout (level 37)
DELETE FROM `quest_template` WHERE `ID`=95147;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (95147,2,16591,0,0,95148,1,1.0,0,1,263931,33554432,0,6130900294268439629,'The Lost Scout','Deliver Bolain''s message to Captain Dalton at Southern Watch.','You... where did you come from? You shouldn''t be here!$B$B<Bolain struggles through the pain--bruised, battered, and a disease-ridden bite wound to his leg.>$B$BTake this to Dalton. Head north up the road and east where the plains open wider than this gash in my damn leg. I''d tell you to tell him he was right, but I won''t give him that satisfaction... at least not until after I''m dead.$B$BI hate these damn hills!','','',70862,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,72,4,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=95147;
DELETE FROM `quest_template_addon` WHERE `ID`=95147;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (95147,0);
DELETE FROM `quest_request_items` WHERE `ID`=95147;
DELETE FROM `quest_offer_reward` WHERE `ID`=95147;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (95147,'Bolain went where?? That son of a--I told him not to go alone.$B$BIt seems some of our own still underestimate those damn gnolls. The knight commander will want to hear of this at once.',0);
DELETE FROM `quest_classic_money` WHERE `ID`=95147;
DELETE FROM `quest_classic_level` WHERE `ID`=95147;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (95147,37,36,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=95147;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (95147,2,37);
DELETE FROM `creature_queststarter` WHERE `quest`=95147;
DELETE FROM `gameobject_queststarter` WHERE `quest`=95147;
DELETE FROM `creature_questender` WHERE `quest`=95147;
DELETE FROM `gameobject_questender` WHERE `quest`=95147;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (260026,95147,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (259353,95147,0);

-- 95148 Southern Watch Gnoll Report (level 37)
DELETE FROM `quest_template` WHERE `ID`=95148;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (95148,2,16591,0,0,0,1,1.0,0,1,268421,33554432,0,6130900294268439629,'Southern Watch Gnoll Report','Deliver the Southern Watch Gnoll Report to the knight commander at Farholde Keep.','As much as I''d like to deliver the blood-soaked message to the knight commander himself, I''d also like to keep my post. I''ve cleaned it up a bit so it''s less... well, bloody.$B$BGiven the recent gnoll uprisings, I''m afraid the task of delivering the news falls upon you.','','',70862,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2826,1,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=95148;
DELETE FROM `quest_template_addon` WHERE `ID`=95148;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (95148,95147);
DELETE FROM `quest_request_items` WHERE `ID`=95148;
DELETE FROM `quest_offer_reward` WHERE `ID`=95148;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (95148,'It is unfortunate news, but it is no surprise. For years the gnoll presence has riddled these lands.$B$BWhen we first arrived the battles raged throughout the hills and lower plains in constant skirmishes. Only after many losses were we able to recover and hold our ground.$B$BI''ll see to it Dalton and his men receive additional rations, weapons, and for the time being, footmen. We can''t risk the gnolls breaching our territory again.',0);
DELETE FROM `quest_classic_money` WHERE `ID`=95148;
DELETE FROM `quest_classic_level` WHERE `ID`=95148;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (95148,37,36,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=95148;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (95148,2,37);
DELETE FROM `creature_queststarter` WHERE `quest`=95148;
DELETE FROM `gameobject_queststarter` WHERE `quest`=95148;
DELETE FROM `creature_questender` WHERE `quest`=95148;
DELETE FROM `gameobject_questender` WHERE `quest`=95148;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (259353,95148,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (257558,95148,0);

-- 95156 Stuff 'Em and Stick 'Em (level 37)  -- item rewards not seen, none given; races/flags as the zones cache quests, rep 2826 INFERRED
DELETE FROM `quest_template` WHERE `ID`=95156;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (95156,2,16591,0,0,0,4,1.0,0,1,0,36700168,0,6130900294268439629,'Stuff ''Em and Stick ''Em','Collect 3 Bundles of Hay for Ember at the training grounds.','Our training dummies are starting to show significant wear and tear. If we don''t fix them up soon I worry our recruits may be unprepared for battle.$B$BIf you hadn''t seen it on your way in, there''s a small farmstead just outside the keep run by a man by the name of Wheeler. He may have a few extra bushels of hay he''d be willing to part with.$B$BI''ll see to it he''s compensated if he''s able to spare supplies for our efforts.','','',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2826,3,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=95156;
DELETE FROM `quest_objectives` WHERE `ID` IN (9900004);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (9900004,95156,1,0,0,268536,3,0,0,0,0,0,1,NULL,0);
DELETE FROM `quest_template_addon` WHERE `ID`=95156;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (95156,0);
DELETE FROM `quest_request_items` WHERE `ID`=95156;
INSERT INTO `quest_request_items` (`ID`,`CompletionText`,`VerifiedBuild`) VALUES (95156,'Find what you were looking for?',0);
DELETE FROM `quest_offer_reward` WHERE `ID`=95156;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (95156,'Thank you, $n.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=95156;
DELETE FROM `quest_classic_level` WHERE `ID`=95156;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (95156,37,34,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=95156;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (95156,2,37);
DELETE FROM `creature_queststarter` WHERE `quest`=95156;
DELETE FROM `gameobject_queststarter` WHERE `quest`=95156;
DELETE FROM `creature_questender` WHERE `quest`=95156;
DELETE FROM `gameobject_questender` WHERE `quest`=95156;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (260080,95156,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (260080,95156,0);

-- 95184 The Sharpest of Blades (level 38)  -- turn-in INFERRED = giver; item 7964 is an old game item players make or buy
DELETE FROM `quest_template` WHERE `ID`=95184;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (95184,2,16591,0,0,0,5,1.0,0,1,0,8,0,6130900294268439629,'The Sharpest of Blades','Bring $1oa Solid Sharpening Stones to Ember Bladewhisper at the Farholde training grounds.','The rigors of training for battle are one thing, but these trainees don''t yet understand the difference between a training sword and the well-kept blade of a seasoned veteran.$B$BI think it''s time we show our recruits how a sharpened blade can be the difference between victory and death.$B$BIf you can find us a few sharpening stones, we''ll put them to good use. There are certain craftsmen known to study this trade. I''d suggest starting there.','','',70862,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2826,5,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=95184;
DELETE FROM `quest_objectives` WHERE `ID` IN (470145);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (470145,95184,1,0,1,7964,8,1434,32,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_template_addon` WHERE `ID`=95184;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (95184,95156);
DELETE FROM `quest_request_items` WHERE `ID`=95184;
DELETE FROM `quest_offer_reward` WHERE `ID`=95184;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (95184,'Thank you, $n.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=95184;
INSERT INTO `quest_classic_money` (`ID`,`RewardMoney`,`RewardMoneyMaxLevel`) VALUES (95184,2000,0);
DELETE FROM `quest_classic_level` WHERE `ID`=95184;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (95184,38,36,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=95184;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (95184,2,38);
DELETE FROM `creature_queststarter` WHERE `quest`=95184;
DELETE FROM `gameobject_queststarter` WHERE `quest`=95184;
DELETE FROM `creature_questender` WHERE `quest`=95184;
DELETE FROM `gameobject_questender` WHERE `quest`=95184;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (260080,95184,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (260080,95184,0);

-- 95219 From Port to Port (level 41)  -- turn-in INFERRED = giver
DELETE FROM `quest_template` WHERE `ID`=95219;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (95219,2,16591,0,0,0,1,1.0,0,1,276068,33554432,0,6130900294268439629,'From Port to Port','Deliver the Cask of Powderfuse Port to Mardrack Greenwell at Feathermoon Stronghold in Feralas.','Got another job for ya, kiddo. How do you feel about traveling the world?$B$BThis here is some of the finest port you ever laid eyes on. Trouble is, it''s gotta go quite the distance to one of my customers all the way over to Feathermoon Stronghold in Feralas. When you''ve reached trees taller than you''ve ever seen before, you know you''ve reached the spot.$B$BOne of my guys packed it airtight but there''s no telling what might set it off, so handle with care, okay bub?','','',70862,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=95219;
DELETE FROM `quest_template_addon` WHERE `ID`=95219;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (95219,0);
DELETE FROM `quest_request_items` WHERE `ID`=95219;
DELETE FROM `quest_offer_reward` WHERE `ID`=95219;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (95219,'Thank you, $n.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=95219;
DELETE FROM `quest_classic_level` WHERE `ID`=95219;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (95219,41,40,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=95219;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (95219,2,41);
DELETE FROM `creature_queststarter` WHERE `quest`=95219;
DELETE FROM `gameobject_queststarter` WHERE `quest`=95219;
DELETE FROM `creature_questender` WHERE `quest`=95219;
DELETE FROM `gameobject_questender` WHERE `quest`=95219;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (255897,95219,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (255897,95219,0);

-- 95254 WANTED: Trapclaw (level 42)
DELETE FROM `quest_template` WHERE `ID`=95254;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (95254,2,16591,0,0,0,5,1.0,0,1,0,36700168,0,18446744073709551615,'WANTED: Trapclaw','Bring Trapclaw''s Pincer to Fimbo Greasemitz.','Calling all anglers!$B$BI''m in the market for some crab meat. Not just the usual shreds either, I''m talking whole claw. I''ve seen a big one crawling around under the docks, stealing bait and cutting lines.$B$BI''ll pay a reward to anyone who brings me that thing''s pincer. You''ll also get a helping of the dish I whip up.$B$B-- Fimbo Greasemitz','','',70862,16766,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2827,6,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=95254;
DELETE FROM `quest_objectives` WHERE `ID` IN (470193);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (470193,95254,1,0,1,268586,1,1434,32,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_template_addon` WHERE `ID`=95254;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (95254,0);
DELETE FROM `quest_request_items` WHERE `ID`=95254;
DELETE FROM `quest_offer_reward` WHERE `ID`=95254;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (95254,'Thank you, $n.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=95254;
INSERT INTO `quest_classic_money` (`ID`,`RewardMoney`,`RewardMoneyMaxLevel`) VALUES (95254,3000,0);
DELETE FROM `quest_classic_level` WHERE `ID`=95254;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (95254,42,36,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=95254;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (95254,2,42);
DELETE FROM `creature_queststarter` WHERE `quest`=95254;
DELETE FROM `gameobject_queststarter` WHERE `quest`=95254;
DELETE FROM `creature_questender` WHERE `quest`=95254;
DELETE FROM `gameobject_questender` WHERE `quest`=95254;
INSERT INTO `gameobject_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (628948,95254,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (259962,95254,0);

-- 95329 Out in the Field (level 38)
DELETE FROM `quest_template` WHERE `ID`=95329;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (95329,2,16591,0,0,0,1,1.0,0,1,0,36700168,0,6130900294268439629,'Out in the Field','Go to Terral''s Watch to get a status report from Watcher Cornelius.','There is a watch tower overlooking the plains to the north. It serves as an observation point to track any motion of the Horde''s forces.$B$BThe watcher stationed there is named Cornelius. Speak to him and get a status report. He is all alone out there, so I like to check on him with some regularity. Fair warning, he can be a bit...taciturn. It''s why he was assigned such a remote post in the first place.','','',70862,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2826,1,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=95329;
DELETE FROM `quest_template_addon` WHERE `ID`=95329;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (95329,94877);
DELETE FROM `quest_request_items` WHERE `ID`=95329;
DELETE FROM `quest_offer_reward` WHERE `ID`=95329;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (95329,'My status? There''s nothing much to report. The plains are lousy with orcs, as always, and the only help Demar seems interested in sending me are greenhorns like yourself.',0);
DELETE FROM `quest_classic_money` WHERE `ID`=95329;
DELETE FROM `quest_classic_level` WHERE `ID`=95329;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (95329,38,36,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=95329;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (95329,2,38);
DELETE FROM `creature_queststarter` WHERE `quest`=95329;
DELETE FROM `gameobject_queststarter` WHERE `quest`=95329;
DELETE FROM `creature_questender` WHERE `quest`=95329;
DELETE FROM `gameobject_questender` WHERE `quest`=95329;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (257558,95329,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (259352,95329,0);

-- 95561 Apple of My Eye (level 38)
DELETE FROM `quest_template` WHERE `ID`=95561;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (95561,2,16591,0,0,95913,3,1.0,0,1,0,36700168,0,6130900294268439629,'Apple of My Eye','Collect $1oa Fresh Apples for Tessa Dawnbright in Farholde Keep.','<The loud cries of baby Dawnbright echo through Farholde Keep. Tessa looks at you with a tired smile.>$B$BOh, hello there adventurer. Sorry for the noise. Little Emma here seems to be quite fussy today, but I''m not sure why. Maybe some food will help.$B$BDo you think you could help me find something for her to eat? My husband stepped out to get some supplies so my hands are a little too full to get them myself. If you could find a few apples around here I could make a lovely mash for her.','','',70862,4539,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2826,5,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=95561;
DELETE FROM `quest_objectives` WHERE `ID` IN (471659);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (471659,95561,1,0,0,271431,6,1766269031,32,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_template_addon` WHERE `ID`=95561;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (95561,0);
DELETE FROM `quest_request_items` WHERE `ID`=95561;
INSERT INTO `quest_request_items` (`ID`,`CompletionText`,`VerifiedBuild`) VALUES (95561,'They never tell you how much parenting is just making it up as you go. Everyone makes it look so easy.',0);
DELETE FROM `quest_offer_reward` WHERE `ID`=95561;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (95561,'Thank you, $n.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=95561;
DELETE FROM `quest_classic_level` WHERE `ID`=95561;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (95561,38,36,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=95561;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (95561,2,38);
DELETE FROM `creature_queststarter` WHERE `quest`=95561;
DELETE FROM `gameobject_queststarter` WHERE `quest`=95561;
DELETE FROM `creature_questender` WHERE `quest`=95561;
DELETE FROM `gameobject_questender` WHERE `quest`=95561;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (263089,95561,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (263089,95561,0);

-- 95603 Bats in the Belfry (level 38)  -- item rewards not seen, none given; races/flags as the zones cache quests, rep 2826 INFERRED
DELETE FROM `quest_template` WHERE `ID`=95603;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (95603,2,16591,0,0,0,4,1.0,0,1,0,36700168,0,6130900294268439629,'Bats in the Belfry','Kill 5 Roosting Duskbats in the Farholde Cathedral for Sister Rowland.','Since brother Lowley and I began serving Farholde in a more secular capacity, the church here has fallen into disuse. I''d like to think that once the fighting is over, we will be able to bring the Light back to this place. Although, I fear that may not be for a long while...$B$BStill, we need to keep the church in working order, in the hope that day will come.$B$BTo that end, a colony of bats has taken to roosting in the upper stories of the church. It would be a great help if you could remove them.','','',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2826,3,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=95603;
DELETE FROM `quest_objectives` WHERE `ID` IN (9900005);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (9900005,95603,0,0,0,262037,5,0,0,0,0,0,1,NULL,0);
DELETE FROM `quest_template_addon` WHERE `ID`=95603;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (95603,0);
DELETE FROM `quest_request_items` WHERE `ID`=95603;
DELETE FROM `quest_offer_reward` WHERE `ID`=95603;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (95603,'You are a good soul, warrior. Thank you.',0);
DELETE FROM `quest_classic_money` WHERE `ID`=95603;
DELETE FROM `quest_classic_level` WHERE `ID`=95603;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (95603,38,35,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=95603;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (95603,2,38);
DELETE FROM `creature_queststarter` WHERE `quest`=95603;
DELETE FROM `gameobject_queststarter` WHERE `quest`=95603;
DELETE FROM `creature_questender` WHERE `quest`=95603;
DELETE FROM `gameobject_questender` WHERE `quest`=95603;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (262031,95603,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (262031,95603,0);

-- 95792 Clear 'em Out (level 42)
DELETE FROM `quest_template` WHERE `ID`=95792;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (95792,2,16591,0,0,0,5,1.0,0,1,0,36700168,0,18446744073709551615,'Clear ''em Out','Kill $1oa Cinderscale Whelps and $2oa Cinderscale Warriors for Gelinda Coppergleam.','If you wanna land a big score, you gotta go where others wouldn''t dare! This cave behind me''s rich with untapped mineral deposits. I''m talkin'' BIG bucks if we can get a crew in there.$B$BThere''s just this... teeny tiny... dragon problem. It''s nothing, really.$B$BIf you''re willing to go in there and save me the hassle of clearing the dragons out, there''s a nice reward in it for you. Not to mention, you''ll be in the good graces of a future mining tycoon!','','',70862,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=95792;
DELETE FROM `quest_objectives` WHERE `ID` IN (471327,471328);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (471327,95792,0,0,2,262473,8,1769103693,32,0,0.0,0,1,NULL,70862),(471328,95792,0,1,3,262477,4,0,32,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_template_addon` WHERE `ID`=95792;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (95792,0);
DELETE FROM `quest_request_items` WHERE `ID`=95792;
DELETE FROM `quest_offer_reward` WHERE `ID`=95792;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (95792,'Terrific work, warrior. Once my little mining venture turns a profit I promise I''ll cut ya in!',0);
DELETE FROM `quest_classic_money` WHERE `ID`=95792;
DELETE FROM `quest_classic_level` WHERE `ID`=95792;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (95792,42,36,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=95792;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (95792,2,42);
DELETE FROM `creature_queststarter` WHERE `quest`=95792;
DELETE FROM `gameobject_queststarter` WHERE `quest`=95792;
DELETE FROM `creature_questender` WHERE `quest`=95792;
DELETE FROM `gameobject_questender` WHERE `quest`=95792;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (262399,95792,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (262399,95792,0);

-- 95913 Fit for a Princess (level 38)  -- turn-in INFERRED = giver; reward item(s) 274931,274932 left out: no item data in our client files yet
DELETE FROM `quest_template` WHERE `ID`=95913;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (95913,2,16591,0,0,95961,5,1.0,0,1,0,36700168,0,6130900294268439629,'Fit for a Princess','Collect $1oa Coyote Pelts for Tessa Dawnbright in Farholde Keep.','<Tessa rocks her baby, trying to help her sleep.>$B$BIf she isn''t hungry, maybe she''s just tired. To be honest, Emma here was a delightful surprise, but that means we were a little unprepared. My husband built her this lovely bassinet, but I worry these blankets are too old. She deserves the best, after all.$B$BIf you''re still willing to help, I would be forever grateful if you could find some pelts for me to line her bed with. If this stops her crying, I think everyone in Farholde would be grateful.','','',70862,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2826,5,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=95913;
DELETE FROM `quest_objectives` WHERE `ID` IN (471660);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (471660,95913,1,0,1,271432,3,0,32,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_template_addon` WHERE `ID`=95913;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (95913,95561);
DELETE FROM `quest_request_items` WHERE `ID`=95913;
DELETE FROM `quest_offer_reward` WHERE `ID`=95913;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (95913,'Thank you, $n.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=95913;
DELETE FROM `quest_classic_level` WHERE `ID`=95913;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (95913,38,36,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=95913;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (95913,2,38);
DELETE FROM `creature_queststarter` WHERE `quest`=95913;
DELETE FROM `gameobject_queststarter` WHERE `quest`=95913;
DELETE FROM `creature_questender` WHERE `quest`=95913;
DELETE FROM `gameobject_questender` WHERE `quest`=95913;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (263089,95913,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (263089,95913,0);

-- 95934 Proof of Profit (level 42)
DELETE FROM `quest_template` WHERE `ID`=95934;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (95934,2,16591,0,0,0,5,1.0,0,1,0,36700168,0,18446744073709551615,'Proof of Profit','Find a piece of Cubic Draconia for Gelinda Coppergleam.','My surveying tools show signs of a never-before-seen mineral in these caves. Cubic draconia, I call it. Probably worth a fortune.$B$BExciting as this is, no one''s gonna come help me dig it up if I can''t show ''em proof that it exists. Sure, I have the readings from my tools, but people like to hold a big shiny crystal, not look at charts.$B$BI''m sure there''s a few crystals poking up from the dirt in there. If you''re willing to dodge a few dragons and get one, you''d really be helpin'' a gal out.','','',70862,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=95934;
DELETE FROM `quest_objectives` WHERE `ID` IN (471677);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (471677,95934,1,0,0,271611,1,1701988128,32,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_template_addon` WHERE `ID`=95934;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (95934,0);
DELETE FROM `quest_request_items` WHERE `ID`=95934;
INSERT INTO `quest_request_items` (`ID`,`CompletionText`,`VerifiedBuild`) VALUES (95934,'To be honest, I''m not sure if cubic draconia can just be yanked outta the ground with your hands. If you have trouble gettin'' some, come back here and I''ll lend you some dynamite.',0);
DELETE FROM `quest_offer_reward` WHERE `ID`=95934;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (95934,'Thank you, $n.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=95934;
DELETE FROM `quest_classic_level` WHERE `ID`=95934;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (95934,42,36,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=95934;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (95934,2,42);
DELETE FROM `creature_queststarter` WHERE `quest`=95934;
DELETE FROM `gameobject_queststarter` WHERE `quest`=95934;
DELETE FROM `creature_questender` WHERE `quest`=95934;
DELETE FROM `gameobject_questender` WHERE `quest`=95934;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (262399,95934,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (262399,95934,0);

-- 95943 Foreman's Plight (level 38)  -- item rewards not seen, none given; races/flags as the zones cache quests, rep 2826 INFERRED
DELETE FROM `quest_template` WHERE `ID`=95943;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (95943,2,16591,0,0,0,4,1.0,0,1,0,36700168,0,6130900294268439629,'Foreman''s Plight','Locate the Foreman''s Crate and return it to Bertrand Ironbrow in Farholde Keep.','G''day, lass. Name''s Bertrand, Bertrand Ironbrow! At yer service.$B$B<Bertrand extends a filthy hand.>$B$BThese fine folk contracted me tae oversee construction of this here keep. What they lack in experience they make up for in spirit! Problem is, I misplaced me tools.$B$BHad a bit of a run in with a few Twilight in a small grove northwest o'' here. Had to choose between me life or me tools. On one hand, I''m alive. On the other... ye see where this is goin''.$B$BThink ye could help find me tools?','','',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2826,3,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=95943;
DELETE FROM `quest_objectives` WHERE `ID` IN (9900006);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (9900006,95943,1,0,0,271620,1,0,0,0,0,0,1,NULL,0);
DELETE FROM `quest_template_addon` WHERE `ID`=95943;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (95943,0);
DELETE FROM `quest_request_items` WHERE `ID`=95943;
INSERT INTO `quest_request_items` (`ID`,`CompletionText`,`VerifiedBuild`) VALUES (95943,'Did ye find me things?',0);
DELETE FROM `quest_offer_reward` WHERE `ID`=95943;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (95943,'Thank you, $n.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=95943;
DELETE FROM `quest_classic_level` WHERE `ID`=95943;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (95943,38,35,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=95943;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (95943,2,38);
DELETE FROM `creature_queststarter` WHERE `quest`=95943;
DELETE FROM `gameobject_queststarter` WHERE `quest`=95943;
DELETE FROM `creature_questender` WHERE `quest`=95943;
DELETE FROM `gameobject_questender` WHERE `quest`=95943;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (260070,95943,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (260070,95943,0);

-- 95984 Song of the Strider (level 39)  -- item rewards not seen, none given; races/flags as the zones cache quests, rep 2826 INFERRED
DELETE FROM `quest_template` WHERE `ID`=95984;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (95984,2,16591,0,0,0,5,1.0,0,1,0,36700168,0,6130900294268439629,'Song of the Strider','Bring an Intact Tallstrider Beak to Watcher Cornelius at Terral''s Watch.','These markings are from a lion. By the look of them, this one''s especially large. The leader of the nearby pride, most likely.$B$BIf rumor spreads about a man-eating lion prowling around on my watch, Demar will be furious. Fortunately, I have a plan.$B$BI can make a whistle that mimics the call of a wounded tallstrider. We can use that to lure the beast out. I''ll just need one of their beaks to give it the right sound. There''s tallstriders all over the plains. Get me a beak, and make it snappy!','','',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2826,3,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=95984;
DELETE FROM `quest_objectives` WHERE `ID` IN (9900007);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (9900007,95984,1,0,0,271778,1,0,0,0,0,0,1,NULL,0);
DELETE FROM `quest_template_addon` WHERE `ID`=95984;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (95984,95983);
DELETE FROM `quest_request_items` WHERE `ID`=95984;
INSERT INTO `quest_request_items` (`ID`,`CompletionText`,`VerifiedBuild`) VALUES (95984,'I think this goes without saying, but you''ll be the one using this whistle.',0);
DELETE FROM `quest_offer_reward` WHERE `ID`=95984;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (95984,'Thank you, $n.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=95984;
DELETE FROM `quest_classic_level` WHERE `ID`=95984;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (95984,39,36,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=95984;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (95984,2,39);
DELETE FROM `creature_queststarter` WHERE `quest`=95984;
DELETE FROM `gameobject_queststarter` WHERE `quest`=95984;
DELETE FROM `creature_questender` WHERE `quest`=95984;
DELETE FROM `gameobject_questender` WHERE `quest`=95984;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (259352,95984,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (259352,95984,0);

-- 96009 High Stakes (level 38)
DELETE FROM `quest_template` WHERE `ID`=96009;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (96009,2,16591,0,0,0,4,1.0,0,1,0,36700168,0,6130900294268439629,'High Stakes','Collect $1oa Wood Bundles from Turner''s Logging Camp for Bertrand Ironbrow in Farholde Keep.','One thing that never ceases to amaze me is the drive of humans. They see a desolate warzone teemin'' with orcs and cultists and figure that''s the perfect spot tae set up shop! Amazin''.$B$BBut, it''s not me job tae tell ''em where to build. Just gotta show ''em how. And if we want this keep tae be as big as they say, we''re gonna need lots o'' wood.$B$BWith how dangerous the roads have become our lumber shipments from the mill tae the northwest are slowin'' to a crawl. Think ye can fetch a few bundles for us?','','',70862,274957,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=96009;
DELETE FROM `quest_objectives` WHERE `ID` IN (471804);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (471804,96009,1,0,0,271877,10,1734962274,32,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_template_addon` WHERE `ID`=96009;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (96009,95943);
DELETE FROM `quest_request_items` WHERE `ID`=96009;
INSERT INTO `quest_request_items` (`ID`,`CompletionText`,`VerifiedBuild`) VALUES (96009,'The mill''s just northwest from here if ye can make it past the crocs an'' murlocs.',0);
DELETE FROM `quest_offer_reward` WHERE `ID`=96009;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (96009,'Thank you, $n.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=96009;
DELETE FROM `quest_classic_level` WHERE `ID`=96009;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (96009,38,36,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=96009;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (96009,2,38);
DELETE FROM `creature_queststarter` WHERE `quest`=96009;
DELETE FROM `gameobject_queststarter` WHERE `quest`=96009;
DELETE FROM `creature_questender` WHERE `quest`=96009;
DELETE FROM `gameobject_questender` WHERE `quest`=96009;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (260070,96009,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (260070,96009,0);

-- 96063 Gaseous Gold (level 38)  -- giver INFERRED = turn-in
DELETE FROM `quest_template` WHERE `ID`=96063;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (96063,2,16591,0,0,0,5,1.0,0,1,0,3145992,0,18446744073709551615,'Gaseous Gold','Gather 5 Marsh Gas Vials for Sleebo Fizzlespout.','The fuel of the future''s not coal, or oil, it''s gas, baby! Those marshy ponds to the southwest spew it out by the ton, and I''ve got just the invention to put it to use--a new type of engine that runs on the stuff. The gas is a real pain to collect, though.$B$BIf you feel like making a humble contribution to the future of science and industry, why don''t you help me get some of that gas? You can see it coming up from vents near the water.','','',70862,274922,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=96063;
DELETE FROM `quest_objectives` WHERE `ID` IN (471899);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (471899,96063,1,0,3,272004,8,1734962274,32,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_template_addon` WHERE `ID`=96063;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (96063,0);
DELETE FROM `quest_request_items` WHERE `ID`=96063;
INSERT INTO `quest_request_items` (`ID`,`CompletionText`,`VerifiedBuild`) VALUES (96063,'I hope you don''t mind if I keep ya on a need-to-know basis about the finer details of my invention. Gotta protect my trade secrets, y''know?',0);
DELETE FROM `quest_offer_reward` WHERE `ID`=96063;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (96063,'Thank you, $n.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=96063;
DELETE FROM `quest_classic_level` WHERE `ID`=96063;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (96063,38,36,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=96063;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (96063,2,38);
DELETE FROM `creature_queststarter` WHERE `quest`=96063;
DELETE FROM `gameobject_queststarter` WHERE `quest`=96063;
DELETE FROM `creature_questender` WHERE `quest`=96063;
DELETE FROM `gameobject_questender` WHERE `quest`=96063;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (263367,96063,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (263367,96063,0);

-- 96125 In Need of Relief (level 43)  -- turn-in INFERRED = giver
DELETE FROM `quest_template` WHERE `ID`=96125;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`FlagsEx`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`,`RewardItem1`,`RewardAmount1`,`RewardItem2`,`RewardAmount2`,`RewardItem3`,`RewardAmount3`,`RewardItem4`,`RewardAmount4`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemDisplayID1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`,`RewardChoiceItemDisplayID2`,`RewardChoiceItemID3`,`RewardChoiceItemQuantity3`,`RewardChoiceItemDisplayID3`,`RewardChoiceItemID4`,`RewardChoiceItemQuantity4`,`RewardChoiceItemDisplayID4`,`RewardChoiceItemID5`,`RewardChoiceItemQuantity5`,`RewardChoiceItemDisplayID5`,`RewardChoiceItemID6`,`RewardChoiceItemQuantity6`,`RewardChoiceItemDisplayID6`,`RewardFactionID1`,`RewardFactionValue1`,`RewardFactionOverride1`,`RewardFactionCapIn1`,`RewardFactionID2`,`RewardFactionValue2`,`RewardFactionOverride2`,`RewardFactionCapIn2`,`RewardFactionID3`,`RewardFactionValue3`,`RewardFactionOverride3`,`RewardFactionCapIn3`,`RewardFactionID4`,`RewardFactionValue4`,`RewardFactionOverride4`,`RewardFactionCapIn4`,`RewardFactionID5`,`RewardFactionValue5`,`RewardFactionOverride5`,`RewardFactionCapIn5`) VALUES (96125,2,16591,0,0,0,1,1.0,0,1,0,36700168,0,6130900294268439629,'In Need of Relief','Find Dunston Willis somewhere among the ogres in Bolder''ok.','Now that the lion hides have been collected, and the orcish poachers culled, I can finally get a break.$B$BAt least, that''s what I would say, if the man who was supposed to relieve me of my watch wasn''t missing.$B$BDunston Willis, explorer extraordinaire, went to investigate the ogre settlement to the north a few days ago. He said he''d take over my post upon his return.$B$BFind him. If he''s alive, bring him back here. If he''s hanging up on meat hooks, bring me proof so Demar can send me someone else.','','',70862,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2826,5,0,7,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0,7);
DELETE FROM `quest_objectives` WHERE `QuestID`=96125;
DELETE FROM `quest_template_addon` WHERE `ID`=96125;
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (96125,0);
DELETE FROM `quest_request_items` WHERE `ID`=96125;
DELETE FROM `quest_offer_reward` WHERE `ID`=96125;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (96125,'Thank you, $n.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=96125;
DELETE FROM `quest_classic_level` WHERE `ID`=96125;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (96125,43,38,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=96125;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (96125,2,43);
DELETE FROM `creature_queststarter` WHERE `quest`=96125;
DELETE FROM `gameobject_queststarter` WHERE `quest`=96125;
DELETE FROM `creature_questender` WHERE `quest`=96125;
DELETE FROM `gameobject_questender` WHERE `quest`=96125;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (259352,96125,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (259352,96125,0);

UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (255897,256438,257558,257591,258023,258317,259049,259050,259051,259158,259352,259353,259961,259962,260026,260070,260080,262031,262399,263089,263367);

-- HEIGHT GUESS: npc 262399 at -8688.2 -3352.9 has no spawn within 250 yd; z is a rough guess 8 yd up, fix with the height pass or in game
-- Left out (need a script, a credit NPC/object we lack, or a giver nobody saw):
--   94615 Foal Play: Find Clues is a scripted event objective; objective '0/1 Find Clues': item id not certain
--   94618 Wheeler's Fleeing Steeds: turned in at one of the fled horses: which NPC and where is not known
--   94679 Snargl's Snout: item 265357: nobody we have drops it
--   94843 From Seed to Sprout: planting the sapling needs the Loamy Soil object and its spell credit (no spot known); object 621767: no template or no spot
--   94857 If a Tree Falls, and No One Hears...: no giver known
--   94861 Helping Hands: objective 'Go with Hurley to save Curly.' (event) needs a script or is not readable
--   94885 Field Marshal's Log: no giver known; objective "0/1 Field Marshal's Log": item id not certain
--   94887 Saving Marshal Ryan: NPC 257250: no template here and no spot in the trace
--   94888 No Soldier Left Behind: rescuing the scouts needs a scripted credit NPC 272737; NPC 272737: no template here and no spot in the trace
--   94889 Carving a Path: NPC 256797: no template here and no spot in the trace
--   94907 A Small Favor: capturing the thrall needs a scripted credit NPC 271293; NPC 271293: no template here and no spot in the trace
--   94925 A Fur Trade: item 266888: nobody we have drops it
--   94963 Biting the Hand: escort of Fuzzle needs an escort script
--   95119 Unfriendly Competition: NPC 257901: no template here and no spot in the trace; NPC 257910: no template here and no spot in the trace
--   95144 Overseas Delivery: starts from item 276065, nobody seen dropping it
--   95288 Smuggler's Paradise: item 268796: nobody we have drops it
--   95330 Love Thy Neighbor: no giver known
--   95426 Rog'mar Supplies: item 274975: nobody we have drops it
--   95613 Keeping the Light On: Candles replaced is a scripted objective
--   95615 In Memoriam: objective '0/3 Candles replaced' (object) needs a script or is not readable
--   95912 WANTED: Blademaster Kaijo: no giver known; item 271425: nobody we have drops it
--   95982 Not My Circus: item 274971: nobody we have drops it
--   95983 A Feast for Buzzards: NPC 263316: no template here and no spot in the trace
--   95986 King of the Wild Plains: item 281052: nobody we have drops it
--   95991 WANTED: Ghostmaw: no giver known; no turn-in known
--   96015 A Miner Problem: NPC 260995: no template here and no spot in the trace; NPC 260996: no template here and no spot in the trace
--   96067 Rock 'em and Sock 'em: fight with G45-B4G needs a scripted event; no giver known; no turn-in known
