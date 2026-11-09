-- Rumbler (248463), the earth elemental Nimsy (248464, Apprentice Geosculptor, Fargodeep Mine) calls when she drops below 60%
-- (her spell 1246059 summons 248463, client spell data). We had no creature_template for it, so the summon did nothing; a player
-- reported (2026-11-08) Rumbler never showing up. Quest Book Return (91775) needs the book from Nimsy (already in her loot).
-- Name, level 6-7, type elemental, normal: Wowhead (Forever) NPC list. LOOK-ALIKE MODEL: the real model is not known yet, so it
-- uses the Stone Rumbler's (4528) model 9589 and its combat values. VerifiedBuild -1 marks the row for the real data from players'
-- cache files (creaturecache.wdb, tools/forever_cache). Faction 17 as Nimsy, so it fights on her side. Safe to run again.
DELETE FROM `creature_template` WHERE `entry`=248463;
INSERT INTO `creature_template` (`entry`,`KillCredit1`,`KillCredit2`,`name`,`femaleName`,`subname`,`TitleAlt`,`IconName`,`RequiredExpansion`,
 `VignetteID`,`faction`,`npcflag`,`speed_walk`,`speed_run`,`scale`,`Classification`,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,
 `RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,`family`,`trainer_class`,`type`,`VehicleId`,`AIName`,`MovementType`,
 `ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,
 `ScriptName`,`StringId`,`VerifiedBuild`)
SELECT 248463,0,0,'Rumbler','','',NULL,NULL,`RequiredExpansion`,0,17,0,`speed_walk`,`speed_run`,`scale`,0,`dmgschool`,`BaseAttackTime`,
 `RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,`family`,0,`type`,0,'',0,
 `ExperienceModifier`,0,`movementId`,0,0,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,'',NULL,-1
FROM `creature_template` WHERE `entry`=4528;
DELETE FROM `creature_template_model` WHERE `CreatureID`=248463;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES (248463,0,9589,1,1,-1);
DELETE FROM `creature_template_difficulty` WHERE `Entry`=248463;
INSERT INTO `creature_template_difficulty` (`Entry`,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,
 `HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,
 `TypeFlags3`,`LootID`,`PickPocketLootID`,`SkinLootID`,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,
 `StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild`)
SELECT 248463,0,0,0,0,0,1,1,1,1,0,`TypeFlags`,0,0,0,0,0,0,0,`StaticFlags1`,0,0,0,0,0,0,0,-1 FROM `creature_template_difficulty` WHERE `Entry`=4528 AND `DifficultyID`=0;
DELETE FROM `creature_classic_level` WHERE `entry`=248463;
INSERT INTO `creature_classic_level` (`entry`,`level_min`,`level_max`) VALUES (248463,6,7);
