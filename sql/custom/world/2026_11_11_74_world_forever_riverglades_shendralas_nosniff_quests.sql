-- Riverglades and Shen'dralas (WoW Forever, Classic 1.60): 19 more quests built WITHOUT a sniff.
-- Nobody has sniffed these zones yet, so this file puts together what other sources have, and marks every guess so a later sniff
-- can replace it piece by piece.
-- Sources (all read as data):
--   * Quest text, levels, objectives, rewards, races and the next quest in each chain: Efficient Games' EG Link export
--     (JIVESCORP/eg-link-output-wowf 9fcae73, quests.enUS, builds up to 1.60.1.70862).
--     Data from Efficient Games (https://efficient.games), licensed under CC BY 4.0.
--     Some quests come from players' live game caches instead (Onlybeams, Cache 2), read with tools/forever_cache.
--   * Who gives and who takes each quest: Questie's Forever data (QuestieDB aca0da1, GPLv3) where it has it, otherwise the quest's
--     own log line ("... for Neza Bloodsnout at Rog'mar"), the item that starts it (the game's ItemSparse.db2), or the NPC that
--     takes the quest before it in the chain.
--   * Which NPCs carry a quest item: the caches (creature quest items), Questie, or the NPCs the log line names.
-- How guesses are marked (swap these first when a sniff has the real thing):
--   * VerifiedBuild -1 on guessed spawns, copied NPC templates and the placeholder reward text.
--   * VerifiedBuild 1 on creature_queststarter / creature_questender rows that are our guess (those columns can't be negative);
--     0 where Questie or an item says who it is.
--   * Quest text and objectives are Blizzard's own record and keep build 70862.
-- Spots: Questie (Wowhead) map points turned into world spots with a fit from our own single-spawn NPCs in the zone
-- (Riverglades 111 NPCs, median 0.6 yd; Shen'dralas 8 NPCs, median 1.8 yd). Heights from our nearest spawn, NPCs 2 yd higher.
-- rg_heights.py (riverglades-0134) can put every spot on the ground from the server's .map files.
-- Drop chances are ESTIMATED: 100 % from NPCs with one or two spawns (named), otherwise 30 % + 3 % per item wanted (at most 80 %).
-- Held back (need a script, an NPC or object nobody has a spot for, or a giver nobody knows): see the list at the end.
-- Safe to run again: every key is deleted first. Guid blocks 47222000-47222199 (creature).

-- A. NPCs we lacked (2)
DELETE FROM `creature_template` WHERE `entry`=253136;
DELETE FROM `creature_template_model` WHERE `CreatureID`=253136;
DELETE FROM `creature_template_difficulty` WHERE `Entry`=253136;
DELETE FROM `creature_classic_level` WHERE `entry`=253136;
-- 253136 Orgrul: no cache record: copy of 253851 Hagiak (Magram centaur at the Magram Front), model too; level 42 (his quests are 40-43)
INSERT INTO `creature_template` (`entry`,`KillCredit1`,`KillCredit2`,`name`,`femaleName`,`subname`,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,`npcflag`,`speed_walk`,`speed_run`,`scale`,`Classification`,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,`family`,`trainer_class`,`type`,`VehicleId`,`AIName`,`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,`ScriptName`,`StringId`,`VerifiedBuild`) SELECT 253136,0,0,'Orgrul',`femaleName`,NULL,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,`npcflag`,`speed_walk`,`speed_run`,`scale`,`Classification`,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,`family`,`trainer_class`,`type`,`VehicleId`,'',`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,'',`StringId`,-1 FROM `creature_template` WHERE `entry`=253851;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) SELECT 253136,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,-1 FROM `creature_template_model` WHERE `CreatureID`=253851;
INSERT INTO `creature_template_difficulty` (`Entry`,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,`LootID`,`PickPocketLootID`,`SkinLootID`,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild`) SELECT 253136,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,0,0,0,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,-1 FROM `creature_template_difficulty` WHERE `Entry`=253851 AND `DifficultyID`=0;
INSERT INTO `creature_classic_level` (`entry`,`level_min`,`level_max`) VALUES (253136,42,42);
DELETE FROM `creature_template` WHERE `entry`=256831;
DELETE FROM `creature_template_model` WHERE `CreatureID`=256831;
DELETE FROM `creature_template_difficulty` WHERE `Entry`=256831;
DELETE FROM `creature_classic_level` WHERE `entry`=256831;
-- 256831 Apprentice Claire: name, model, type, class, elite and health from the cache; copy of 257591 Ashe Amberhall (Farholde Keep, Alliance)
INSERT INTO `creature_template` (`entry`,`KillCredit1`,`KillCredit2`,`name`,`femaleName`,`subname`,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,`npcflag`,`speed_walk`,`speed_run`,`scale`,`Classification`,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`unit_flags3`,`family`,`trainer_class`,`type`,`VehicleId`,`AIName`,`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,`ScriptName`,`StringId`,`VerifiedBuild`) SELECT 256831,0,0,'Apprentice Claire',`femaleName`,NULL,`TitleAlt`,`IconName`,`RequiredExpansion`,`VignetteID`,`faction`,`npcflag`,`speed_walk`,`speed_run`,`scale`,0,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,`BaseVariance`,`RangeVariance`,8,`unit_flags`,`unit_flags2`,`unit_flags3`,0,`trainer_class`,7,`VehicleId`,'',`MovementType`,`ExperienceModifier`,`RacialLeader`,`movementId`,`WidgetSetID`,`WidgetSetUnitConditionID`,`RegenHealth`,`CreatureImmunitiesId`,`flags_extra`,'',`StringId`,-1 FROM `creature_template` WHERE `entry`=257591;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES (256831,0,148705,1.0,1.0,70862);
INSERT INTO `creature_template_difficulty` (`Entry`,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,`LootID`,`PickPocketLootID`,`SkinLootID`,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild`) SELECT 256831,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,1.0,1.0,`ArmorModifier`,`DamageModifier`,339891,134217728,0,`TypeFlags3`,0,0,0,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,-1 FROM `creature_template_difficulty` WHERE `Entry`=257591 AND `DifficultyID`=0;
INSERT INTO `creature_classic_level` (`entry`,`level_min`,`level_max`) VALUES (256831,35,35);

-- B. Spawns for them (ESTIMATED spots, see header): 253136: 2 spot(s) from Questie; 256831: 1 spot(s) from Questie; extra copies (ESTIMATED, same height + 2): 253431 Gelkis Captive: we had 1, a quest wants 5, +2 within 22 yd of ours; 255769 Redridge Brute: we had 4, a quest wants 40, +4 within 22 yd of ours; 256383 Meadowsbrook Farmhand: we had 3, a quest wants 12, +3 within 22 yd of ours; 256434 Turner's Mill Lumberjack: we had 1, a quest wants 10, +4 within 22 yd of ours; 257926 Farholde Scout: we had 4, a quest wants 12, +2 within 22 yd of ours; 270667 Farholde Priest: we had 2, a quest wants 8, +2 within 22 yd of ours
DELETE FROM `creature` WHERE `guid` BETWEEN 47222000 AND 47222199;
INSERT INTO `creature` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`modelid`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`currentwaypoint`,`curHealthPct`,`MovementType`,`npcflag`,`unit_flags`,`unit_flags2`,`unit_flags3`,`ScriptName`,`StringId`,`VerifiedBuild`) VALUES
(47222000,253136,1,0,0,'0',0,0,0,-1,0,0,-2065.01,1295.64,66.46,1.37,300,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,-1),
(47222001,253136,1,0,0,'0',0,0,0,-1,0,0,-2078.63,1291.55,66.46,2.16,300,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,-1),
(47222002,256831,0,0,0,'0',0,0,0,-1,0,0,-7993.09,-4200.29,29.91,4.03,300,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,-1),
(47222003,253431,1,0,0,'0',0,0,0,-1,0,0,-2273.49,786.90,147.65,3.17,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,-1),
(47222004,253431,1,0,0,'0',0,0,0,-1,0,0,-2258.36,797.63,147.65,2.12,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,-1),
(47222005,255769,0,0,0,'0',0,0,0,-1,0,0,-8652.18,-2846.50,190.85,3.97,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,-1),
(47222006,255769,0,0,0,'0',0,0,0,-1,0,0,-8614.77,-2990.73,190.71,1.71,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,-1),
(47222007,255769,0,0,0,'0',0,0,0,-1,0,0,-8591.92,-3022.49,190.93,2.18,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,-1),
(47222008,255769,0,0,0,'0',0,0,0,-1,0,0,-8423.93,-3365.12,169.60,3.99,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,-1),
(47222009,256383,0,0,0,'0',0,0,0,-1,0,0,-9039.57,-4085.21,24.55,5.57,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,-1),
(47222010,256383,0,0,0,'0',0,0,0,-1,0,0,-8976.11,-4081.89,27.42,0.07,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,-1),
(47222011,256383,0,0,0,'0',0,0,0,-1,0,0,-9168.27,-4142.72,28.07,6.21,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,-1),
(47222012,256434,0,0,0,'0',0,0,0,-1,0,0,-8348.01,-3927.96,16.19,0.20,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,-1),
(47222013,256434,0,0,0,'0',0,0,0,-1,0,0,-8339.78,-3923.60,16.19,4.44,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,-1),
(47222014,256434,0,0,0,'0',0,0,0,-1,0,0,-8327.58,-3940.42,16.19,1.02,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,-1),
(47222015,256434,0,0,0,'0',0,0,0,-1,0,0,-8361.69,-3950.82,16.19,3.53,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,-1),
(47222016,257926,0,0,0,'0',0,0,0,-1,0,0,-8508.56,-4013.58,18.78,4.21,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,-1),
(47222017,257926,0,0,0,'0',0,0,0,-1,0,0,-8722.40,-4398.78,26.84,2.88,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,-1),
(47222018,270667,0,0,0,'0',0,0,0,-1,0,0,-8607.46,-4104.94,26.29,1.68,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,-1),
(47222019,270667,0,0,0,'0',0,0,0,-1,0,0,-8738.45,-4380.91,25.11,0.79,300,5,0,100,1,NULL,NULL,NULL,NULL,'',NULL,-1);

-- C. Quest drops (only for players on the quest)
-- 265745 Twilight Missive from 254560 Twilight Shadowmancer (cache/Questie drop list), quest 94680, 39 % ESTIMATED chance
UPDATE `creature_template_difficulty` SET `LootID`=254560 WHERE `Entry`=254560 AND `DifficultyID`=0 AND `LootID`=0;
DELETE FROM `creature_loot_template` WHERE `Entry`=(SELECT `LootID` FROM `creature_template_difficulty` WHERE `Entry`=254560 AND `DifficultyID`=0) AND `ItemType`=0 AND `Item`=265745;
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) SELECT `LootID`,0,265745,39,1,1,0,1,1,'Twilight Shadowmancer - quests 94680' FROM `creature_template_difficulty` WHERE `Entry`=254560 AND `DifficultyID`=0;
-- 265358 Stolen Ore from 255769 Redridge Brute (Stolen Ore: "from gnolls in the Bristle Hills", so all three of our Redridge gnoll kinds there (our guess)), quest 94535, 80 % ESTIMATED chance
UPDATE `creature_template_difficulty` SET `LootID`=255769 WHERE `Entry`=255769 AND `DifficultyID`=0 AND `LootID`=0;
DELETE FROM `creature_loot_template` WHERE `Entry`=(SELECT `LootID` FROM `creature_template_difficulty` WHERE `Entry`=255769 AND `DifficultyID`=0) AND `ItemType`=0 AND `Item`=265358;
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) SELECT `LootID`,0,265358,80,1,1,0,1,1,'Redridge Brute - quests 94535' FROM `creature_template_difficulty` WHERE `Entry`=255769 AND `DifficultyID`=0;
-- 265358 Stolen Ore from 255770 Redridge Mystic (Stolen Ore: "from gnolls in the Bristle Hills", so all three of our Redridge gnoll kinds there (our guess)), quest 94535, 80 % ESTIMATED chance
UPDATE `creature_template_difficulty` SET `LootID`=255770 WHERE `Entry`=255770 AND `DifficultyID`=0 AND `LootID`=0;
DELETE FROM `creature_loot_template` WHERE `Entry`=(SELECT `LootID` FROM `creature_template_difficulty` WHERE `Entry`=255770 AND `DifficultyID`=0) AND `ItemType`=0 AND `Item`=265358;
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) SELECT `LootID`,0,265358,80,1,1,0,1,1,'Redridge Mystic - quests 94535' FROM `creature_template_difficulty` WHERE `Entry`=255770 AND `DifficultyID`=0;
-- 265358 Stolen Ore from 255772 Redridge Trapper (Stolen Ore: "from gnolls in the Bristle Hills", so all three of our Redridge gnoll kinds there (our guess)), quest 94535, 80 % ESTIMATED chance
UPDATE `creature_template_difficulty` SET `LootID`=255772 WHERE `Entry`=255772 AND `DifficultyID`=0 AND `LootID`=0;
DELETE FROM `creature_loot_template` WHERE `Entry`=(SELECT `LootID` FROM `creature_template_difficulty` WHERE `Entry`=255772 AND `DifficultyID`=0) AND `ItemType`=0 AND `Item`=265358;
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) SELECT `LootID`,0,265358,80,1,1,0,1,1,'Redridge Trapper - quests 94535' FROM `creature_template_difficulty` WHERE `Entry`=255772 AND `DifficultyID`=0;
-- 769 Chunk of Boar Meat from 255851 Mature Paletusk (Chunk of Boar Meat: the Mature Paletusks around Bolder'ok (our guess)), quest 95162, 60 % ESTIMATED chance
UPDATE `creature_template_difficulty` SET `LootID`=255851 WHERE `Entry`=255851 AND `DifficultyID`=0 AND `LootID`=0;
DELETE FROM `creature_loot_template` WHERE `Entry`=(SELECT `LootID` FROM `creature_template_difficulty` WHERE `Entry`=255851 AND `DifficultyID`=0) AND `ItemType`=0 AND `Item`=769;
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) SELECT `LootID`,0,769,60,1,1,0,1,1,'Mature Paletusk - quests 95162' FROM `creature_template_difficulty` WHERE `Entry`=255851 AND `DifficultyID`=0;
-- 263972 Woodworker's Axe from 256434 Turner's Mill Lumberjack (cache/Questie drop list), quest 93788, 100 %
UPDATE `creature_template_difficulty` SET `LootID`=256434 WHERE `Entry`=256434 AND `DifficultyID`=0 AND `LootID`=0;
DELETE FROM `creature_loot_template` WHERE `Entry`=(SELECT `LootID` FROM `creature_template_difficulty` WHERE `Entry`=256434 AND `DifficultyID`=0) AND `ItemType`=0 AND `Item`=263972;
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) SELECT `LootID`,0,263972,100,1,1,0,1,1,'Turner''s Mill Lumberjack - quests 93788' FROM `creature_template_difficulty` WHERE `Entry`=256434 AND `DifficultyID`=0;
-- 271793 Plains Buzzard Feather from 263343 Low Plains Buzzard (cache/Questie drop list), quest 96107, 48 % ESTIMATED chance
UPDATE `creature_template_difficulty` SET `LootID`=263343 WHERE `Entry`=263343 AND `DifficultyID`=0 AND `LootID`=0;
DELETE FROM `creature_loot_template` WHERE `Entry`=(SELECT `LootID` FROM `creature_template_difficulty` WHERE `Entry`=263343 AND `DifficultyID`=0) AND `ItemType`=0 AND `Item`=271793;
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) SELECT `LootID`,0,271793,48,1,1,0,1,1,'Low Plains Buzzard - quests 96107' FROM `creature_template_difficulty` WHERE `Entry`=263343 AND `DifficultyID`=0;

-- D. Quests
-- 92841 Valley of Bones (Shen_Dralas, all): giver NPC 5398 (Questie); turn-in NPC 253136 (Questie)
DELETE FROM `quest_template` WHERE `ID`=92841;
INSERT INTO `quest_template` (`ID`,`QuestType`,`LogTitle`,`VerifiedBuild`) VALUES (92841,2,'Valley of Bones',-1);
DELETE FROM `quest_classic_level` WHERE `ID`=92841;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (92841,0,0,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=92841;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (92841,0,0);
DELETE FROM `quest_template_addon` WHERE `ID`=92841;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (92841,0,0,0);
DELETE FROM `quest_offer_reward` WHERE `ID`=92841;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (92841,'Well done, $n. Thank you.',-1);
DELETE FROM `creature_queststarter` WHERE `quest`=92841;
DELETE FROM `gameobject_queststarter` WHERE `quest`=92841;
DELETE FROM `creature_questender` WHERE `quest`=92841;
DELETE FROM `gameobject_questender` WHERE `quest`=92841;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (5398,92841,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (253136,92841,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (5398,253136);
UPDATE `quest_template` SET `QuestDescription`='It is time, outsider, for you to learn of our great discovery... but Warug will not be the one to tell it!\n\nHead south to the other end of the village. Look for a big, stinky centaur named Orgrul.\n\nYou will know when you get there.\nYou will know when you see him.\nIf he thinks you deserve to know, you will know.\n\nAnd if he doesn''t? Well. You can be happy knowing Warug has a new crown.', `LogDescription`='Improve your reputation with the Magram, and locate Orgrul near the Valley of Bones south of Magram Village.', `AreaDescription`='', `QuestCompletionLog`='', `LogTitle`='Valley of Bones', `QuestSortID`=16651, `QuestInfoID`=0, `SuggestedGroupNum`=0, `RewardNextQuest`=92844, `RewardXPDifficulty`=1, `RewardXPMultiplier`=1.0, `RewardMoneyDifficulty`=0, `RewardMoneyMultiplier`=1.0, `RewardSpell`=0, `StartItem`=0, `Flags`=36700168, `FlagsEx`=0, `AllowableRaces`=18446744073709551615, `TimeAllowed`=0, `VerifiedBuild`=70862, `RewardFactionID1`=93, `RewardFactionValue1`=0, `RewardFactionOverride1`=12500, `RewardFactionCapIn1`=0, `RewardFactionID2`=92, `RewardFactionValue2`=0, `RewardFactionOverride2`=-62500, `RewardFactionCapIn2`=0 WHERE `ID`=92841 AND `VerifiedBuild`=-1;
DELETE FROM `quest_objectives` WHERE `QuestID`=92841;
DELETE FROM `quest_objectives` WHERE `ID` IN (465669);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (465669,92841,6,0,-1,93,9000,0,0,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_classic_money` WHERE `ID`=92841;
UPDATE `quest_classic_level` SET `QuestLevel`=40, `MinLevel`=35 WHERE `ID`=92841;
UPDATE `quest_template_classic_level` SET `LevelType`=2, `QuestLevel`=40 WHERE `ID`=92841;
UPDATE `quest_template` SET `QuestType`=2 WHERE `ID`=92841;

-- 92893 No Mercy for the Weak (Shen_Dralas, all): giver NPC 253136 (same NPC as the turn-in (ESTIMATED)); turn-in NPC 253136 (quest log line)
DELETE FROM `quest_template` WHERE `ID`=92893;
INSERT INTO `quest_template` (`ID`,`QuestType`,`LogTitle`,`VerifiedBuild`) VALUES (92893,2,'No Mercy for the Weak',-1);
DELETE FROM `quest_classic_level` WHERE `ID`=92893;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (92893,0,0,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=92893;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (92893,0,0);
DELETE FROM `quest_template_addon` WHERE `ID`=92893;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (92893,0,0,0);
DELETE FROM `quest_offer_reward` WHERE `ID`=92893;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (92893,'Well done, $n. Thank you.',-1);
DELETE FROM `creature_queststarter` WHERE `quest`=92893;
DELETE FROM `gameobject_queststarter` WHERE `quest`=92893;
DELETE FROM `creature_questender` WHERE `quest`=92893;
DELETE FROM `gameobject_questender` WHERE `quest`=92893;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (253136,92893,1);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (253136,92893,1);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (253136);
UPDATE `quest_template` SET `QuestDescription`='As you tread further into the Valley of Bones, you will find Gelkis wanderers that dared to enter our territory. I instructed they be kept alive so they could feed the bone worms burrowing about, but they seem uninterested in their prey being alive.\n\nThis task is simple. Find the prisoners, then snuff out their life. Don''t give them a brief glimmer of hope for freedom.', `LogDescription`='Maintain your reputation with the Magram Clan, and slay $1oa Gelkis Captives scattered around the Valley of Bones. Return to Orgrul after the task is complete.', `AreaDescription`='', `QuestCompletionLog`='', `LogTitle`='No Mercy for the Weak', `QuestSortID`=16651, `QuestInfoID`=0, `SuggestedGroupNum`=0, `RewardNextQuest`=93066, `RewardXPDifficulty`=4, `RewardXPMultiplier`=1.0, `RewardMoneyDifficulty`=0, `RewardMoneyMultiplier`=1.0, `RewardSpell`=0, `StartItem`=0, `Flags`=36700168, `FlagsEx`=0, `AllowableRaces`=18446744073709551615, `TimeAllowed`=0, `VerifiedBuild`=70862, `RewardFactionID1`=93, `RewardFactionValue1`=0, `RewardFactionOverride1`=50000, `RewardFactionCapIn1`=0, `RewardFactionID2`=92, `RewardFactionValue2`=0, `RewardFactionOverride2`=-250000, `RewardFactionCapIn2`=0 WHERE `ID`=92893 AND `VerifiedBuild`=-1;
DELETE FROM `quest_objectives` WHERE `QuestID`=92893;
DELETE FROM `quest_objectives` WHERE `ID` IN (465781,465782);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (465781,92893,0,0,0,253431,5,0,0,0,0.0,0,1,NULL,70862),(465782,92893,6,1,-1,93,9000,0,0,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_classic_money` WHERE `ID`=92893;
UPDATE `quest_classic_level` SET `QuestLevel`=42, `MinLevel`=35 WHERE `ID`=92893;
UPDATE `quest_template_classic_level` SET `LevelType`=2, `QuestLevel`=42 WHERE `ID`=92893;
UPDATE `quest_template` SET `QuestType`=2 WHERE `ID`=92893;

-- 93786 A Message in Blood (Riverglades, H): giver NPC 256418 (Questie); turn-in NPC 256418 (quest log line)
DELETE FROM `quest_template` WHERE `ID`=93786;
INSERT INTO `quest_template` (`ID`,`QuestType`,`LogTitle`,`VerifiedBuild`) VALUES (93786,2,'A Message in Blood',-1);
DELETE FROM `quest_classic_level` WHERE `ID`=93786;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (93786,0,0,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=93786;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (93786,0,0);
DELETE FROM `quest_template_addon` WHERE `ID`=93786;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (93786,0,0,0);
DELETE FROM `quest_offer_reward` WHERE `ID`=93786;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (93786,'Well done, $n. Thank you.',-1);
DELETE FROM `creature_queststarter` WHERE `quest`=93786;
DELETE FROM `gameobject_queststarter` WHERE `quest`=93786;
DELETE FROM `creature_questender` WHERE `quest`=93786;
DELETE FROM `gameobject_questender` WHERE `quest`=93786;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (256418,93786,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (256418,93786,1);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (256418);
UPDATE `quest_template` SET `QuestDescription`='Does your blade thirst for human blood, $n?\n\nSome years ago, the human defectors laid claim to a small farm to the southwest across the river. With the area rich in lumber and stone, it was only a matter of time before they expanded their reach to the keep along the eastern shore known as Farholde.\n\nOur first skirmishes would have ended in our favor if we''d had the numbers. It''s time to remind them of our presence.\n\nDisrupt their farming efforts. Take no prisoners. Send them a clear message.', `LogDescription`='Slay $1oa Meadowsbrook Farmhands, then return to Krom''rosh with news of your success.', `AreaDescription`='', `QuestCompletionLog`='', `LogTitle`='A Message in Blood', `QuestSortID`=16591, `QuestInfoID`=0, `SuggestedGroupNum`=0, `RewardNextQuest`=93787, `RewardXPDifficulty`=5, `RewardXPMultiplier`=1.0, `RewardMoneyDifficulty`=0, `RewardMoneyMultiplier`=1.0, `RewardSpell`=0, `StartItem`=0, `Flags`=36700168, `FlagsEx`=0, `AllowableRaces`=12261800583900083122, `TimeAllowed`=0, `VerifiedBuild`=70862 WHERE `ID`=93786 AND `VerifiedBuild`=-1;
DELETE FROM `quest_objectives` WHERE `QuestID`=93786;
DELETE FROM `quest_objectives` WHERE `ID` IN (467473);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (467473,93786,0,0,0,256383,12,0,0,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_classic_money` WHERE `ID`=93786;
UPDATE `quest_classic_level` SET `QuestLevel`=36, `MinLevel`=35 WHERE `ID`=93786;
UPDATE `quest_template_classic_level` SET `LevelType`=2, `QuestLevel`=36 WHERE `ID`=93786;
UPDATE `quest_template` SET `QuestType`=2 WHERE `ID`=93786;

-- 93788 Borrowed Blades (Riverglades, H): giver NPC 256657 (Questie); turn-in NPC 256657 (Questie)
DELETE FROM `quest_template` WHERE `ID`=93788;
INSERT INTO `quest_template` (`ID`,`QuestType`,`LogTitle`,`VerifiedBuild`) VALUES (93788,2,'Borrowed Blades',-1);
DELETE FROM `quest_classic_level` WHERE `ID`=93788;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (93788,0,0,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=93788;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (93788,0,0);
DELETE FROM `quest_template_addon` WHERE `ID`=93788;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (93788,0,0,0);
DELETE FROM `quest_offer_reward` WHERE `ID`=93788;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (93788,'Well done, $n. Thank you.',-1);
DELETE FROM `creature_queststarter` WHERE `quest`=93788;
DELETE FROM `gameobject_queststarter` WHERE `quest`=93788;
DELETE FROM `creature_questender` WHERE `quest`=93788;
DELETE FROM `gameobject_questender` WHERE `quest`=93788;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (256657,93788,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (256657,93788,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (256657);
UPDATE `quest_template` SET `QuestDescription`='With the human presence to the south and Twilight to the west, the battle for resources has become difficult. But why gather resources amongst our enemies when their blades will suffice?\n\nFollow the river south to the mill. When you hear the whistling of woodworkers among the trees, tread lightly. When the opportunity presents itself, strike hard and fast.\n\nI''ll stay behind to keep the forge running hot. Gur''dok will break down the blades while I fashion weaponry forged of their own materials.', `LogDescription`='Retrieve $1oa Woodworker''s Axe from the lumberjacks at Turner''s Logging Camp for Kreza Darkthorn in Rog''mar.', `AreaDescription`='', `QuestCompletionLog`='', `LogTitle`='Borrowed Blades', `QuestSortID`=16591, `QuestInfoID`=0, `SuggestedGroupNum`=0, `RewardNextQuest`=93976, `RewardXPDifficulty`=5, `RewardXPMultiplier`=1.0, `RewardMoneyDifficulty`=0, `RewardMoneyMultiplier`=1.0, `RewardSpell`=0, `StartItem`=0, `Flags`=36700168, `FlagsEx`=0, `AllowableRaces`=12261800583900083122, `TimeAllowed`=0, `VerifiedBuild`=70862 WHERE `ID`=93788 AND `VerifiedBuild`=-1;
DELETE FROM `quest_objectives` WHERE `QuestID`=93788;
DELETE FROM `quest_objectives` WHERE `ID` IN (467479);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (467479,93788,1,0,0,263972,10,0,0,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_classic_money` WHERE `ID`=93788;
UPDATE `quest_classic_level` SET `QuestLevel`=38, `MinLevel`=36 WHERE `ID`=93788;
UPDATE `quest_template_classic_level` SET `LevelType`=2, `QuestLevel`=38 WHERE `ID`=93788;
UPDATE `quest_template` SET `QuestType`=2 WHERE `ID`=93788;

-- 94483 Show Them How It's Done (Riverglades, H): giver NPC 256418 (Questie); turn-in NPC 256418 (Questie)
DELETE FROM `quest_template` WHERE `ID`=94483;
INSERT INTO `quest_template` (`ID`,`QuestType`,`LogTitle`,`VerifiedBuild`) VALUES (94483,2,'Show Them How It''s Done',-1);
DELETE FROM `quest_classic_level` WHERE `ID`=94483;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (94483,0,0,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=94483;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (94483,0,0);
DELETE FROM `quest_template_addon` WHERE `ID`=94483;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (94483,0,0,0);
DELETE FROM `quest_offer_reward` WHERE `ID`=94483;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (94483,'Well done, $n. Thank you.',-1);
DELETE FROM `creature_queststarter` WHERE `quest`=94483;
DELETE FROM `gameobject_queststarter` WHERE `quest`=94483;
DELETE FROM `creature_questender` WHERE `quest`=94483;
DELETE FROM `gameobject_questender` WHERE `quest`=94483;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (256418,94483,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (256418,94483,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (256418);
UPDATE `quest_template` SET `QuestDescription`='Our scouts have reported training camps by the river to the south. If I''d received word sooner, it would have saved us both the hassle of locating and removing the human filth in a timely manner.\n\nContinue your efforts, $n. Holding back the humans to their precious little keep will buy us time to focus on other matters at hand.\n\nBe swift about it.', `LogDescription`='Slay $1oa Farholde Priests and $2oa Farholde Scouts south of the river.', `AreaDescription`='', `QuestCompletionLog`='', `LogTitle`='Show Them How It''s Done', `QuestSortID`=16591, `QuestInfoID`=0, `SuggestedGroupNum`=0, `RewardNextQuest`=0, `RewardXPDifficulty`=5, `RewardXPMultiplier`=1.0, `RewardMoneyDifficulty`=0, `RewardMoneyMultiplier`=1.0, `RewardSpell`=0, `StartItem`=0, `Flags`=36700168, `FlagsEx`=0, `AllowableRaces`=12261800583900083122, `TimeAllowed`=0, `VerifiedBuild`=70862 WHERE `ID`=94483 AND `VerifiedBuild`=-1;
DELETE FROM `quest_objectives` WHERE `QuestID`=94483;
DELETE FROM `quest_objectives` WHERE `ID` IN (476938,468778);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (476938,94483,0,0,0,270667,8,0,0,0,0.0,0,1,NULL,70862),(468778,94483,0,1,1,257926,12,0,0,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_classic_money` WHERE `ID`=94483;
INSERT INTO `quest_classic_money` (`ID`,`RewardMoney`,`RewardMoneyMaxLevel`) VALUES (94483,3500,0);
UPDATE `quest_classic_level` SET `QuestLevel`=39, `MinLevel`=36 WHERE `ID`=94483;
UPDATE `quest_template_classic_level` SET `LevelType`=2, `QuestLevel`=39 WHERE `ID`=94483;
UPDATE `quest_template` SET `QuestType`=2 WHERE `ID`=94483;

-- 94532 Gnoll Infestation (Riverglades, H): giver NPC 258031 (same NPC as the turn-in (ESTIMATED)); turn-in NPC 258031 (quest log line)
DELETE FROM `quest_template` WHERE `ID`=94532;
INSERT INTO `quest_template` (`ID`,`QuestType`,`LogTitle`,`VerifiedBuild`) VALUES (94532,2,'Gnoll Infestation',-1);
DELETE FROM `quest_classic_level` WHERE `ID`=94532;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (94532,0,0,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=94532;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (94532,0,0);
DELETE FROM `quest_template_addon` WHERE `ID`=94532;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (94532,0,0,0);
DELETE FROM `quest_offer_reward` WHERE `ID`=94532;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (94532,'Well done, $n. Thank you.',-1);
DELETE FROM `creature_queststarter` WHERE `quest`=94532;
DELETE FROM `gameobject_queststarter` WHERE `quest`=94532;
DELETE FROM `creature_questender` WHERE `quest`=94532;
DELETE FROM `gameobject_questender` WHERE `quest`=94532;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (258031,94532,1);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (258031,94532,1);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (258031);
UPDATE `quest_template` SET `QuestDescription`='Though the gnoll presence has dwindled with the Twilight''s encroachment, the threat remains.\n\nThe hills to the southwest are infested with their kind, disrupting trades and travelers alike.\n\nClear a path for our allies by removing the threat.', `LogDescription`='Kill $1oa Redridge Mystics, $2oa Redridge Trappers, and $3oa Redridge Brutes for Neza Bloodsnout at Rog''mar.', `AreaDescription`='', `QuestCompletionLog`='', `LogTitle`='Gnoll Infestation', `QuestSortID`=16591, `QuestInfoID`=0, `SuggestedGroupNum`=0, `RewardNextQuest`=0, `RewardXPDifficulty`=5, `RewardXPMultiplier`=1.0, `RewardMoneyDifficulty`=0, `RewardMoneyMultiplier`=1.0, `RewardSpell`=0, `StartItem`=0, `Flags`=36700168, `FlagsEx`=0, `AllowableRaces`=12261800583900083122, `TimeAllowed`=0, `VerifiedBuild`=70862 WHERE `ID`=94532 AND `VerifiedBuild`=-1;
DELETE FROM `quest_objectives` WHERE `QuestID`=94532;
DELETE FROM `quest_objectives` WHERE `ID` IN (468849,468850,468851);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (468849,94532,0,0,0,255770,8,0,0,0,0.0,0,1,NULL,70862),(468850,94532,0,1,1,255772,8,0,0,0,0.0,0,1,NULL,70862),(468851,94532,0,2,2,255769,6,0,0,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_classic_money` WHERE `ID`=94532;
UPDATE `quest_classic_level` SET `QuestLevel`=38, `MinLevel`=36 WHERE `ID`=94532;
UPDATE `quest_template_classic_level` SET `LevelType`=2, `QuestLevel`=38 WHERE `ID`=94532;
UPDATE `quest_template` SET `QuestType`=2 WHERE `ID`=94532;

-- 94533 Snargl's Snout (Riverglades, H): giver NPC 258031 (same NPC as the turn-in (ESTIMATED)); turn-in NPC 258031 (quest log line)
DELETE FROM `quest_template` WHERE `ID`=94533;
INSERT INTO `quest_template` (`ID`,`QuestType`,`LogTitle`,`VerifiedBuild`) VALUES (94533,2,'Snargl''s Snout',-1);
DELETE FROM `quest_classic_level` WHERE `ID`=94533;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (94533,0,0,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=94533;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (94533,0,0);
DELETE FROM `quest_template_addon` WHERE `ID`=94533;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (94533,0,0,0);
DELETE FROM `quest_offer_reward` WHERE `ID`=94533;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (94533,'Well done, $n. Thank you.',-1);
DELETE FROM `creature_queststarter` WHERE `quest`=94533;
DELETE FROM `gameobject_queststarter` WHERE `quest`=94533;
DELETE FROM `creature_questender` WHERE `quest`=94533;
DELETE FROM `gameobject_questender` WHERE `quest`=94533;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (258031,94533,1);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (258031,94533,1);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (258031);
UPDATE `quest_template` SET `QuestDescription`='Among the cackling inhabitants of the Bristle Hills southwest of here is an elder gnoll by the name of Snargl. The sounds emanating from this particular gnoll are quite unpleasant. Their presence inspires those around, creating a problem for allies and scouts who enter the area.\n\nA trophy as a memento of our having dealt with this most unpleasant gnoll fiend would fit nicely with my collection.\n\nBe swift about it, $n.', `LogDescription`='Locate Snargl in the Bristle Hills, kill him, and retrieve his snout for Neza Bloodsnout.', `AreaDescription`='', `QuestCompletionLog`='', `LogTitle`='Snargl''s Snout', `QuestSortID`=16591, `QuestInfoID`=1, `SuggestedGroupNum`=0, `RewardNextQuest`=0, `RewardXPDifficulty`=5, `RewardXPMultiplier`=1.0, `RewardMoneyDifficulty`=0, `RewardMoneyMultiplier`=1.0, `RewardSpell`=0, `StartItem`=0, `Flags`=36700168, `FlagsEx`=0, `AllowableRaces`=12261800583900083122, `TimeAllowed`=0, `VerifiedBuild`=70862, `RewardChoiceItemID1`=274942, `RewardChoiceItemQuantity1`=1, `RewardChoiceItemDisplayID1`=0, `RewardChoiceItemID2`=274943, `RewardChoiceItemQuantity2`=1, `RewardChoiceItemDisplayID2`=0, `RewardChoiceItemID3`=274944, `RewardChoiceItemQuantity3`=1, `RewardChoiceItemDisplayID3`=0, `RewardChoiceItemID4`=274946, `RewardChoiceItemQuantity4`=1, `RewardChoiceItemDisplayID4`=0 WHERE `ID`=94533 AND `VerifiedBuild`=-1;
DELETE FROM `quest_objectives` WHERE `QuestID`=94533;
DELETE FROM `quest_objectives` WHERE `ID` IN (468852);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (468852,94533,1,0,3,265357,1,0,0,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_classic_money` WHERE `ID`=94533;
UPDATE `quest_classic_level` SET `QuestLevel`=40, `MinLevel`=36 WHERE `ID`=94533;
UPDATE `quest_template_classic_level` SET `LevelType`=2, `QuestLevel`=40 WHERE `ID`=94533;
UPDATE `quest_template` SET `QuestType`=2 WHERE `ID`=94533;

-- 94535 Bristle Hills Bandits (Riverglades, H): giver NPC 256657 (same NPC as the turn-in (ESTIMATED)); turn-in NPC 256657 (Questie)
DELETE FROM `quest_template` WHERE `ID`=94535;
INSERT INTO `quest_template` (`ID`,`QuestType`,`LogTitle`,`VerifiedBuild`) VALUES (94535,2,'Bristle Hills Bandits',-1);
DELETE FROM `quest_classic_level` WHERE `ID`=94535;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (94535,0,0,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=94535;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (94535,0,0);
DELETE FROM `quest_template_addon` WHERE `ID`=94535;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (94535,0,0,0);
DELETE FROM `quest_offer_reward` WHERE `ID`=94535;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (94535,'Well done, $n. Thank you.',-1);
DELETE FROM `creature_queststarter` WHERE `quest`=94535;
DELETE FROM `gameobject_queststarter` WHERE `quest`=94535;
DELETE FROM `creature_questender` WHERE `quest`=94535;
DELETE FROM `gameobject_questender` WHERE `quest`=94535;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (256657,94535,1);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (256657,94535,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (256657);
UPDATE `quest_template` SET `QuestDescription`='Our allies mining ore from the hills to the south reported several attacks by gnolls in the area.\n\nTravel south, locate their camps and retrieve the stolen ore. Take it from their dead bodies if need be.', `LogDescription`='Gather $1oa Stolen Ore from gnolls in the Bristle Hills for Kreza Darkthorn.', `AreaDescription`='', `QuestCompletionLog`='', `LogTitle`='Bristle Hills Bandits', `QuestSortID`=16591, `QuestInfoID`=0, `SuggestedGroupNum`=0, `RewardNextQuest`=0, `RewardXPDifficulty`=5, `RewardXPMultiplier`=1.0, `RewardMoneyDifficulty`=0, `RewardMoneyMultiplier`=1.0, `RewardSpell`=0, `StartItem`=0, `Flags`=36700168, `FlagsEx`=0, `AllowableRaces`=12261800583900083122, `TimeAllowed`=0, `VerifiedBuild`=70862 WHERE `ID`=94535 AND `VerifiedBuild`=-1;
DELETE FROM `quest_objectives` WHERE `QuestID`=94535;
DELETE FROM `quest_objectives` WHERE `ID` IN (468853);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (468853,94535,1,0,3,265358,40,0,0,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_classic_money` WHERE `ID`=94535;
INSERT INTO `quest_classic_money` (`ID`,`RewardMoney`,`RewardMoneyMaxLevel`) VALUES (94535,4500,0);
UPDATE `quest_classic_level` SET `QuestLevel`=38, `MinLevel`=36 WHERE `ID`=94535;
UPDATE `quest_template_classic_level` SET `LevelType`=2, `QuestLevel`=38 WHERE `ID`=94535;
UPDATE `quest_template` SET `QuestType`=2 WHERE `ID`=94535;

-- 94618 Wheeler's Fleeing Steeds (Riverglades, A): giver NPC 258317 (Questie); turn-in NPC 259254 (Questie)
DELETE FROM `quest_template` WHERE `ID`=94618;
INSERT INTO `quest_template` (`ID`,`QuestType`,`LogTitle`,`VerifiedBuild`) VALUES (94618,2,'Wheeler''s Fleeing Steeds',-1);
DELETE FROM `quest_classic_level` WHERE `ID`=94618;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (94618,0,0,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=94618;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (94618,0,0);
DELETE FROM `quest_template_addon` WHERE `ID`=94618;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (94618,0,0,0);
DELETE FROM `quest_offer_reward` WHERE `ID`=94618;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (94618,'Well done, $n. Thank you.',-1);
DELETE FROM `creature_queststarter` WHERE `quest`=94618;
DELETE FROM `gameobject_queststarter` WHERE `quest`=94618;
DELETE FROM `creature_questender` WHERE `quest`=94618;
DELETE FROM `gameobject_questender` WHERE `quest`=94618;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (258317,94618,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (259254,94618,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (258317,259254);
UPDATE `quest_template` SET `QuestDescription`='From what you found, it seems like the coyotes got most of the horses. Still, there''s a few not accounted for.\n\nThose that didn''t get eaten probably ran off scared someplace. I don''t know how many of those horses there are, but we''d better find ''em before the coyotes come back.\n\n<Benjy holds out a sack in front of you.>\n\nHere. If you find one, give it some of our feed. That''ll get it to trust you so you can lead it back here.', `LogDescription`='Find one of the horses that fled Wheeler''s Grange after the coyote attack and give it some feed.', `AreaDescription`='', `QuestCompletionLog`='', `LogTitle`='Wheeler''s Fleeing Steeds', `QuestSortID`=16591, `QuestInfoID`=0, `SuggestedGroupNum`=0, `RewardNextQuest`=0, `RewardXPDifficulty`=3, `RewardXPMultiplier`=1.0, `RewardMoneyDifficulty`=0, `RewardMoneyMultiplier`=1.0, `RewardSpell`=0, `StartItem`=267178, `Flags`=36700160, `FlagsEx`=0, `AllowableRaces`=6130900294268439629, `TimeAllowed`=0, `VerifiedBuild`=70862, `RewardItem1`=0, `RewardAmount1`=0, `ItemDrop1`=0, `ItemDropQuantity1`=0, `RewardItem2`=0, `RewardAmount2`=0, `ItemDrop2`=0, `ItemDropQuantity2`=0, `RewardItem3`=0, `RewardAmount3`=0, `ItemDrop3`=0, `ItemDropQuantity3`=0, `RewardItem4`=0, `RewardAmount4`=0, `ItemDrop4`=0, `ItemDropQuantity4`=0, `RewardChoiceItemID1`=0, `RewardChoiceItemQuantity1`=0, `RewardChoiceItemDisplayID1`=0, `RewardChoiceItemID2`=0, `RewardChoiceItemQuantity2`=0, `RewardChoiceItemDisplayID2`=0, `RewardChoiceItemID3`=0, `RewardChoiceItemQuantity3`=0, `RewardChoiceItemDisplayID3`=0, `RewardChoiceItemID4`=0, `RewardChoiceItemQuantity4`=0, `RewardChoiceItemDisplayID4`=0, `RewardChoiceItemID5`=0, `RewardChoiceItemQuantity5`=0, `RewardChoiceItemDisplayID5`=0, `RewardChoiceItemID6`=0, `RewardChoiceItemQuantity6`=0, `RewardChoiceItemDisplayID6`=0, `RewardFactionID1`=2826, `RewardFactionValue1`=3, `RewardFactionOverride1`=0, `RewardFactionCapIn1`=7, `RewardFactionID2`=0, `RewardFactionValue2`=0, `RewardFactionOverride2`=0, `RewardFactionCapIn2`=7, `RewardFactionID3`=0, `RewardFactionValue3`=0, `RewardFactionOverride3`=0, `RewardFactionCapIn3`=7, `RewardFactionID4`=0, `RewardFactionValue4`=0, `RewardFactionOverride4`=0, `RewardFactionCapIn4`=7, `RewardFactionID5`=0, `RewardFactionValue5`=0, `RewardFactionOverride5`=0, `RewardFactionCapIn5`=7 WHERE `ID`=94618 AND `VerifiedBuild`=-1;
DELETE FROM `quest_objectives` WHERE `QuestID`=94618;
DELETE FROM `quest_objectives` WHERE `ID` IN (469542);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (469542,94618,1,0,0,267178,1,1434,32,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_classic_money` WHERE `ID`=94618;
UPDATE `quest_classic_level` SET `QuestLevel`=38, `MinLevel`=36 WHERE `ID`=94618;
UPDATE `quest_template_classic_level` SET `LevelType`=2, `QuestLevel`=38 WHERE `ID`=94618;
UPDATE `quest_template` SET `QuestType`=2 WHERE `ID`=94618;

-- 94672 Encroaching Twilight (Riverglades, H): giver NPC 256710 (same NPC as the turn-in (ESTIMATED)); turn-in NPC 256710 (quest log line)
DELETE FROM `quest_template` WHERE `ID`=94672;
INSERT INTO `quest_template` (`ID`,`QuestType`,`LogTitle`,`VerifiedBuild`) VALUES (94672,2,'Encroaching Twilight',-1);
DELETE FROM `quest_classic_level` WHERE `ID`=94672;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (94672,0,0,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=94672;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (94672,0,0);
DELETE FROM `quest_template_addon` WHERE `ID`=94672;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (94672,0,0,0);
DELETE FROM `quest_offer_reward` WHERE `ID`=94672;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (94672,'Well done, $n. Thank you.',-1);
DELETE FROM `creature_queststarter` WHERE `quest`=94672;
DELETE FROM `gameobject_queststarter` WHERE `quest`=94672;
DELETE FROM `creature_questender` WHERE `quest`=94672;
DELETE FROM `gameobject_questender` WHERE `quest`=94672;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (256710,94672,1);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (256710,94672,1);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (256710);
UPDATE `quest_template` SET `QuestDescription`='Humans are not our only adversary within the Riverglades, $n, nor are they our most dire.\n\nA cult known as the Twilight''s Hammer has begun a campaign against those they deem heretics in the eyes of their twisted gods. The ravaged remains of a human settlement to the west are but the first to fall to their conquest. No doubt Rog''mar will be next.\n\nWe cannot afford to let them establish a foothold so close to our doorstep. Go to this so-called Sunnyglade and thin their numbers.', `LogDescription`='Defeat $1oa Twilight Raiders and $2oa Twilight Neophytes within Sunnyglade for Tran''gul in Rog''mar.', `AreaDescription`='', `QuestCompletionLog`='', `LogTitle`='Encroaching Twilight', `QuestSortID`=16591, `QuestInfoID`=0, `SuggestedGroupNum`=0, `RewardNextQuest`=0, `RewardXPDifficulty`=5, `RewardXPMultiplier`=1.0, `RewardMoneyDifficulty`=0, `RewardMoneyMultiplier`=1.0, `RewardSpell`=0, `StartItem`=0, `Flags`=36700168, `FlagsEx`=0, `AllowableRaces`=12261800583900083122, `TimeAllowed`=0, `VerifiedBuild`=70862 WHERE `ID`=94672 AND `VerifiedBuild`=-1;
DELETE FROM `quest_objectives` WHERE `QuestID`=94672;
DELETE FROM `quest_objectives` WHERE `ID` IN (469068,471359,469069);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (469068,94672,0,0,0,256952,8,0,0,0,0.0,0,1,NULL,70862),(471359,94672,0,1,2,254560,8,0,0,0,0.0,0,1,NULL,70862),(469069,94672,0,2,1,256798,8,0,0,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_classic_money` WHERE `ID`=94672;
INSERT INTO `quest_classic_money` (`ID`,`RewardMoney`,`RewardMoneyMaxLevel`) VALUES (94672,5500,0);
UPDATE `quest_classic_level` SET `QuestLevel`=41, `MinLevel`=36 WHERE `ID`=94672;
UPDATE `quest_template_classic_level` SET `LevelType`=2, `QuestLevel`=41 WHERE `ID`=94672;
UPDATE `quest_template` SET `QuestType`=2 WHERE `ID`=94672;

-- 94680 Twilight Missives (Riverglades, H): giver NPC 256710 (same NPC as the turn-in (ESTIMATED)); turn-in NPC 256710 (quest log line)
DELETE FROM `quest_template` WHERE `ID`=94680;
INSERT INTO `quest_template` (`ID`,`QuestType`,`LogTitle`,`VerifiedBuild`) VALUES (94680,2,'Twilight Missives',-1);
DELETE FROM `quest_classic_level` WHERE `ID`=94680;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (94680,0,0,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=94680;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (94680,0,0);
DELETE FROM `quest_template_addon` WHERE `ID`=94680;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (94680,0,0,0);
DELETE FROM `quest_offer_reward` WHERE `ID`=94680;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (94680,'Well done, $n. Thank you.',-1);
DELETE FROM `creature_queststarter` WHERE `quest`=94680;
DELETE FROM `gameobject_queststarter` WHERE `quest`=94680;
DELETE FROM `creature_questender` WHERE `quest`=94680;
DELETE FROM `gameobject_questender` WHERE `quest`=94680;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (256710,94680,1);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (256710,94680,1);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (256710);
UPDATE `quest_template` SET `QuestDescription`='There have always been small pockets of Twilight''s Hammer cultists here and there, but never have I seen them in such numbers. They seem emboldened by someone, or something, to expand their influence.\n\nI would know more of these cultists and what they seek. Their shadowmancers are blessed by the dark powers and are likely the most trusted within their order to lead attacks of this scale.\n\nBring me anything you find off their corpses that we may glean knowledge into their campaign.', `LogDescription`='Retrieve $1oa Twilight Missives from Twilight Shadowmancers in Sunnyglade for Tran''gul in Rog''mar.', `AreaDescription`='', `QuestCompletionLog`='', `LogTitle`='Twilight Missives', `QuestSortID`=16591, `QuestInfoID`=0, `SuggestedGroupNum`=0, `RewardNextQuest`=94727, `RewardXPDifficulty`=4, `RewardXPMultiplier`=1.0, `RewardMoneyDifficulty`=0, `RewardMoneyMultiplier`=1.0, `RewardSpell`=0, `StartItem`=0, `Flags`=36700168, `FlagsEx`=0, `AllowableRaces`=12261800583900083122, `TimeAllowed`=0, `VerifiedBuild`=70862 WHERE `ID`=94680 AND `VerifiedBuild`=-1;
DELETE FROM `quest_objectives` WHERE `QuestID`=94680;
DELETE FROM `quest_objectives` WHERE `ID` IN (469086);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (469086,94680,1,0,2,265745,3,0,0,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_classic_money` WHERE `ID`=94680;
UPDATE `quest_classic_level` SET `QuestLevel`=41, `MinLevel`=36 WHERE `ID`=94680;
UPDATE `quest_template_classic_level` SET `LevelType`=2, `QuestLevel`=41 WHERE `ID`=94680;
UPDATE `quest_template` SET `QuestType`=2 WHERE `ID`=94680;

-- 94857 If a Tree Falls, and No One Hears... (Riverglades, A): giver NPC 258935 (Questie); turn-in NPC 258963 (Questie)
DELETE FROM `quest_template` WHERE `ID`=94857;
INSERT INTO `quest_template` (`ID`,`QuestType`,`LogTitle`,`VerifiedBuild`) VALUES (94857,2,'If a Tree Falls, and No One Hears...',-1);
DELETE FROM `quest_classic_level` WHERE `ID`=94857;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (94857,0,0,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=94857;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (94857,0,0);
DELETE FROM `quest_template_addon` WHERE `ID`=94857;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (94857,0,0,0);
DELETE FROM `quest_offer_reward` WHERE `ID`=94857;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (94857,'Well done, $n. Thank you.',-1);
DELETE FROM `creature_queststarter` WHERE `quest`=94857;
DELETE FROM `gameobject_queststarter` WHERE `quest`=94857;
DELETE FROM `creature_questender` WHERE `quest`=94857;
DELETE FROM `gameobject_questender` WHERE `quest`=94857;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (258935,94857,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (258963,94857,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (258935,258963);
UPDATE `quest_template` SET `QuestDescription`='Stranger, listen to me!\n\nI work as a lumberjack for the nearby logging camp. As you can see, I''ve been trapped here in an accident. If I don''t get out from here soon, I''ll either bleed out, or be eaten by wild animals.\n\nPlease, go to the logging camp and fetch help!', `LogDescription`='Go to Turner''s Logging Camp and get help.', `AreaDescription`='', `QuestCompletionLog`='', `LogTitle`='If a Tree Falls, and No One Hears...', `QuestSortID`=16591, `QuestInfoID`=0, `SuggestedGroupNum`=0, `RewardNextQuest`=0, `RewardXPDifficulty`=1, `RewardXPMultiplier`=1.0, `RewardMoneyDifficulty`=0, `RewardMoneyMultiplier`=1.0, `RewardSpell`=0, `StartItem`=0, `Flags`=36700168, `FlagsEx`=0, `AllowableRaces`=6130900294268439629, `TimeAllowed`=0, `VerifiedBuild`=70862, `RewardFactionID1`=2826, `RewardFactionValue1`=1, `RewardFactionOverride1`=0, `RewardFactionCapIn1`=0 WHERE `ID`=94857 AND `VerifiedBuild`=-1;
DELETE FROM `quest_objectives` WHERE `QuestID`=94857;
DELETE FROM `quest_classic_money` WHERE `ID`=94857;
UPDATE `quest_classic_level` SET `QuestLevel`=39, `MinLevel`=36 WHERE `ID`=94857;
UPDATE `quest_template_classic_level` SET `LevelType`=2, `QuestLevel`=39 WHERE `ID`=94857;
UPDATE `quest_template` SET `QuestType`=2 WHERE `ID`=94857;

-- 94884 An Unsettling Discovery (Riverglades, A): giver NPC 256831 (Questie); turn-in NPC 257591 (Questie)
DELETE FROM `quest_template` WHERE `ID`=94884;
INSERT INTO `quest_template` (`ID`,`QuestType`,`LogTitle`,`VerifiedBuild`) VALUES (94884,2,'An Unsettling Discovery',-1);
DELETE FROM `quest_classic_level` WHERE `ID`=94884;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (94884,0,0,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=94884;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (94884,0,0);
DELETE FROM `quest_template_addon` WHERE `ID`=94884;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (94884,0,0,0);
DELETE FROM `quest_offer_reward` WHERE `ID`=94884;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (94884,'Well done, $n. Thank you.',-1);
DELETE FROM `creature_queststarter` WHERE `quest`=94884;
DELETE FROM `gameobject_queststarter` WHERE `quest`=94884;
DELETE FROM `creature_questender` WHERE `quest`=94884;
DELETE FROM `gameobject_questender` WHERE `quest`=94884;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (256831,94884,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (257591,94884,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (256831,257591);
UPDATE `quest_template` SET `QuestDescription`='I''ve had enough of this cave. Once you clear your way out, I''ll slip out amidst the chaos.\n\nI''m afraid I''m in too weak of condition to carry those samples, however. Their energy is overwhelming, and I''ve been trapped down here for Light knows how long. Once we get out of here, please take them to Lady Amberhall immediately. I must rest.', `LogDescription`='Bring the Filled Crystal Catalyst to Ashe Amberhall in Farholde Keep.', `AreaDescription`='', `QuestCompletionLog`='', `LogTitle`='An Unsettling Discovery', `QuestSortID`=16591, `QuestInfoID`=0, `SuggestedGroupNum`=0, `RewardNextQuest`=0, `RewardXPDifficulty`=3, `RewardXPMultiplier`=1.0, `RewardMoneyDifficulty`=0, `RewardMoneyMultiplier`=1.0, `RewardSpell`=0, `StartItem`=266426, `Flags`=36700160, `FlagsEx`=0, `AllowableRaces`=6130900294268439629, `TimeAllowed`=0, `VerifiedBuild`=70862, `RewardFactionID1`=2826, `RewardFactionValue1`=5, `RewardFactionOverride1`=0, `RewardFactionCapIn1`=0 WHERE `ID`=94884 AND `VerifiedBuild`=-1;
DELETE FROM `quest_objectives` WHERE `QuestID`=94884;
DELETE FROM `quest_objectives` WHERE `ID` IN (469298);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (469298,94884,1,0,2,266426,1,0,0,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_classic_money` WHERE `ID`=94884;
UPDATE `quest_classic_level` SET `QuestLevel`=41, `MinLevel`=36 WHERE `ID`=94884;
UPDATE `quest_template_classic_level` SET `LevelType`=2, `QuestLevel`=41 WHERE `ID`=94884;
UPDATE `quest_template` SET `QuestType`=2 WHERE `ID`=94884;

-- 95062 Frazzlespark's Crate (Riverglades, H): giver NPC 255897 (Questie); turn-in NPC 256418 (quest log line)
DELETE FROM `quest_template` WHERE `ID`=95062;
INSERT INTO `quest_template` (`ID`,`QuestType`,`LogTitle`,`VerifiedBuild`) VALUES (95062,2,'Frazzlespark''s Crate',-1);
DELETE FROM `quest_classic_level` WHERE `ID`=95062;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (95062,0,0,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=95062;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (95062,0,0);
DELETE FROM `quest_template_addon` WHERE `ID`=95062;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (95062,0,0,0);
DELETE FROM `quest_offer_reward` WHERE `ID`=95062;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (95062,'Well done, $n. Thank you.',-1);
DELETE FROM `creature_queststarter` WHERE `quest`=95062;
DELETE FROM `gameobject_queststarter` WHERE `quest`=95062;
DELETE FROM `creature_questender` WHERE `quest`=95062;
DELETE FROM `gameobject_questender` WHERE `quest`=95062;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (255897,95062,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (256418,95062,1);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (255897,256418);
UPDATE `quest_template` SET `QuestDescription`='Time for a special delivery.\n\nOld Krom''rosh is waiting for this package over at Rog''mar just west of here.\n\nI suggest you get it to him quickly, but carefully. Why, you might ask? It''s better to not ask questions.', `LogDescription`='Deliver Frazzlespark''s Crate to Krom''rosh at Rog''mar.', `AreaDescription`='', `QuestCompletionLog`='', `LogTitle`='Frazzlespark''s Crate', `QuestSortID`=16591, `QuestInfoID`=0, `SuggestedGroupNum`=0, `RewardNextQuest`=0, `RewardXPDifficulty`=1, `RewardXPMultiplier`=1.0, `RewardMoneyDifficulty`=0, `RewardMoneyMultiplier`=1.0, `RewardSpell`=0, `StartItem`=276066, `Flags`=36700160, `FlagsEx`=0, `AllowableRaces`=12261800583900083122, `TimeAllowed`=0, `VerifiedBuild`=70862, `RewardFactionID1`=76, `RewardFactionValue1`=1, `RewardFactionOverride1`=0, `RewardFactionCapIn1`=0 WHERE `ID`=95062 AND `VerifiedBuild`=-1;
DELETE FROM `quest_objectives` WHERE `QuestID`=95062;
DELETE FROM `quest_classic_money` WHERE `ID`=95062;
UPDATE `quest_classic_level` SET `QuestLevel`=36, `MinLevel`=36 WHERE `ID`=95062;
UPDATE `quest_template_classic_level` SET `LevelType`=2, `QuestLevel`=36 WHERE `ID`=95062;
UPDATE `quest_template` SET `QuestType`=2 WHERE `ID`=95062;

-- 95162 Paletusk Harvesting (Riverglades, H): giver NPC 256728 (same NPC as the turn-in (ESTIMATED)); turn-in NPC 256728 (quest log line)
DELETE FROM `quest_template` WHERE `ID`=95162;
INSERT INTO `quest_template` (`ID`,`QuestType`,`LogTitle`,`VerifiedBuild`) VALUES (95162,2,'Paletusk Harvesting',-1);
DELETE FROM `quest_classic_level` WHERE `ID`=95162;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (95162,0,0,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=95162;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (95162,0,0);
DELETE FROM `quest_template_addon` WHERE `ID`=95162;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (95162,0,0,0);
DELETE FROM `quest_offer_reward` WHERE `ID`=95162;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (95162,'Well done, $n. Thank you.',-1);
DELETE FROM `creature_queststarter` WHERE `quest`=95162;
DELETE FROM `gameobject_queststarter` WHERE `quest`=95162;
DELETE FROM `creature_questender` WHERE `quest`=95162;
DELETE FROM `gameobject_questender` WHERE `quest`=95162;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (256728,95162,1);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (256728,95162,1);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (256728);
UPDATE `quest_template` SET `QuestDescription`='With so little land not claimed by the humans or Twilight''s Hammer, this ranch is one of our very few sources of food. Ogres aren''t much in the way of farmers, but they love a good paletusk roast, and have agreed to let me stay so long as the meat keeps coming.\n\nAs our war advances, the demand for food is only ever increasing. Now I hardly have enough food to keep these ogres from looking at me like I''m their next meal.\n\nIf we want to have enough meat for Rog''mar, I''ll need your help with the harvest.', `LogDescription`='Gather $1oa Chunks of Boar Meat for Na''zok near Bolder''ok.', `AreaDescription`='', `QuestCompletionLog`='', `LogTitle`='Paletusk Harvesting', `QuestSortID`=16591, `QuestInfoID`=0, `SuggestedGroupNum`=0, `RewardNextQuest`=0, `RewardXPDifficulty`=5, `RewardXPMultiplier`=1.0, `RewardMoneyDifficulty`=0, `RewardMoneyMultiplier`=1.0, `RewardSpell`=0, `StartItem`=0, `Flags`=3145736, `FlagsEx`=0, `AllowableRaces`=12261800583900083122, `TimeAllowed`=0, `VerifiedBuild`=70862, `RewardItem1`=274977, `RewardAmount1`=1, `ItemDrop1`=0, `ItemDropQuantity1`=0 WHERE `ID`=95162 AND `VerifiedBuild`=-1;
DELETE FROM `quest_objectives` WHERE `QuestID`=95162;
DELETE FROM `quest_objectives` WHERE `ID` IN (470094);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (470094,95162,1,0,0,769,10,0,0,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_classic_money` WHERE `ID`=95162;
UPDATE `quest_classic_level` SET `QuestLevel`=41, `MinLevel`=38 WHERE `ID`=95162;
UPDATE `quest_template_classic_level` SET `LevelType`=2, `QuestLevel`=41 WHERE `ID`=95162;
UPDATE `quest_template` SET `QuestType`=2 WHERE `ID`=95162;

-- 95428 Clugfist's Decree (Riverglades, H): giver NPC 255829 (Questie); turn-in NPC 256418 (Questie)
DELETE FROM `quest_template` WHERE `ID`=95428;
INSERT INTO `quest_template` (`ID`,`QuestType`,`LogTitle`,`VerifiedBuild`) VALUES (95428,2,'Clugfist''s Decree',-1);
DELETE FROM `quest_classic_level` WHERE `ID`=95428;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (95428,0,0,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=95428;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (95428,0,0);
DELETE FROM `quest_template_addon` WHERE `ID`=95428;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (95428,0,0,0);
DELETE FROM `quest_offer_reward` WHERE `ID`=95428;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (95428,'Well done, $n. Thank you.',-1);
DELETE FROM `creature_queststarter` WHERE `quest`=95428;
DELETE FROM `gameobject_queststarter` WHERE `quest`=95428;
DELETE FROM `creature_questender` WHERE `quest`=95428;
DELETE FROM `gameobject_questender` WHERE `quest`=95428;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (255829,95428,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (256418,95428,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (255829,256418);
UPDATE `quest_template` SET `QuestDescription`='Little $r smash Twilight good. Maybe your Horde stronger than Clugfist think.\n\nYou free to stay in Bolder''ok. For now.\n\nTell other tinies we spill blood again soon.', `LogDescription`='Report to Krom''rosh in Rog''mar.', `AreaDescription`='', `QuestCompletionLog`='', `LogTitle`='Clugfist''s Decree', `QuestSortID`=16591, `QuestInfoID`=0, `SuggestedGroupNum`=0, `RewardNextQuest`=94744, `RewardXPDifficulty`=1, `RewardXPMultiplier`=1.0, `RewardMoneyDifficulty`=0, `RewardMoneyMultiplier`=1.0, `RewardSpell`=0, `StartItem`=0, `Flags`=36700168, `FlagsEx`=0, `AllowableRaces`=12261800583900083122, `TimeAllowed`=0, `VerifiedBuild`=70862 WHERE `ID`=95428 AND `VerifiedBuild`=-1;
DELETE FROM `quest_objectives` WHERE `QuestID`=95428;
DELETE FROM `quest_classic_money` WHERE `ID`=95428;
UPDATE `quest_classic_level` SET `QuestLevel`=41, `MinLevel`=36 WHERE `ID`=95428;
UPDATE `quest_template_classic_level` SET `LevelType`=2, `QuestLevel`=41 WHERE `ID`=95428;
UPDATE `quest_template` SET `QuestType`=2 WHERE `ID`=95428;

-- 95957 Drink Up! (Riverglades, A): giver NPC 255894 (ender of 95988, which leads to it); turn-in NPC 263120 (quest log line); after 95988
DELETE FROM `quest_template` WHERE `ID`=95957;
INSERT INTO `quest_template` (`ID`,`QuestType`,`LogTitle`,`VerifiedBuild`) VALUES (95957,2,'Drink Up!',-1);
DELETE FROM `quest_classic_level` WHERE `ID`=95957;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (95957,0,0,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=95957;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (95957,0,0);
DELETE FROM `quest_template_addon` WHERE `ID`=95957;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (95957,0,0,95988);
DELETE FROM `quest_offer_reward` WHERE `ID`=95957;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (95957,'Well done, $n. Thank you.',-1);
DELETE FROM `creature_queststarter` WHERE `quest`=95957;
DELETE FROM `gameobject_queststarter` WHERE `quest`=95957;
DELETE FROM `creature_questender` WHERE `quest`=95957;
DELETE FROM `gameobject_questender` WHERE `quest`=95957;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (255894,95957,1);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (263120,95957,1);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (255894,263120);
UPDATE `quest_template` SET `QuestDescription`='Here ya go, champ. One drink for ol'' grumpy guts over there. If this doesn''t sober him up, nothin'' will.\n\nOh and uh... wash your hands after touchin'' this. And don''t get it in your eyes.', `LogDescription`='Deliver Zizplink''s Frothing Mug to Gray Dawnbright in Powderfuse Port.', `AreaDescription`='', `QuestCompletionLog`='', `LogTitle`='Drink Up!', `QuestSortID`=16591, `QuestInfoID`=0, `SuggestedGroupNum`=0, `RewardNextQuest`=95975, `RewardXPDifficulty`=1, `RewardXPMultiplier`=1.0, `RewardMoneyDifficulty`=0, `RewardMoneyMultiplier`=1.0, `RewardSpell`=0, `StartItem`=271822, `Flags`=36700160, `FlagsEx`=0, `AllowableRaces`=6130900294268439629, `TimeAllowed`=0, `VerifiedBuild`=70862, `RewardFactionID1`=2826, `RewardFactionValue1`=5, `RewardFactionOverride1`=0, `RewardFactionCapIn1`=0 WHERE `ID`=95957 AND `VerifiedBuild`=-1;
DELETE FROM `quest_objectives` WHERE `QuestID`=95957;
DELETE FROM `quest_objectives` WHERE `ID` IN (471716);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (471716,95957,1,0,4,271822,1,0,0,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_classic_money` WHERE `ID`=95957;
UPDATE `quest_classic_level` SET `QuestLevel`=38, `MinLevel`=36 WHERE `ID`=95957;
UPDATE `quest_template_classic_level` SET `LevelType`=2, `QuestLevel`=38 WHERE `ID`=95957;
UPDATE `quest_template` SET `QuestType`=2 WHERE `ID`=95957;

-- 95961 Stepped Out (Riverglades, A): giver NPC 263089 (ender of 95913, which leads to it); turn-in NPC 263120 (quest log line); after 95913
DELETE FROM `quest_template` WHERE `ID`=95961;
INSERT INTO `quest_template` (`ID`,`QuestType`,`LogTitle`,`VerifiedBuild`) VALUES (95961,2,'Stepped Out',-1);
DELETE FROM `quest_classic_level` WHERE `ID`=95961;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (95961,0,0,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=95961;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (95961,0,0);
DELETE FROM `quest_template_addon` WHERE `ID`=95961;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (95961,0,0,95913);
DELETE FROM `quest_offer_reward` WHERE `ID`=95961;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (95961,'Well done, $n. Thank you.',-1);
DELETE FROM `creature_queststarter` WHERE `quest`=95961;
DELETE FROM `gameobject_queststarter` WHERE `quest`=95961;
DELETE FROM `creature_questender` WHERE `quest`=95961;
DELETE FROM `gameobject_questender` WHERE `quest`=95961;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (263089,95961,1);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (263120,95961,1);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (263089,263120);
UPDATE `quest_template` SET `QuestDescription`='<Tessa''s baby continues to wail loudly.>\n\nI guess she isn''t tired. Oh... everyone makes it look so easy. Her father always knows how to cheer her up. If only he''d come home already.\n\n$n, I hate to ask this, but can you see what is taking him so long? He set out a while ago but he should have been back by now. Emma just lights up when she sees him.\n\nYou should find him at Powderfuse Port-- he seems to go there a lot lately. You''ll know my Gray when you see him. He shines as brightly as the sun.', `LogDescription`='Find Gray Dawnbright in Powderfuse Port.', `AreaDescription`='', `QuestCompletionLog`='', `LogTitle`='Stepped Out', `QuestSortID`=16591, `QuestInfoID`=0, `SuggestedGroupNum`=0, `RewardNextQuest`=0, `RewardXPDifficulty`=1, `RewardXPMultiplier`=1.0, `RewardMoneyDifficulty`=0, `RewardMoneyMultiplier`=1.0, `RewardSpell`=0, `StartItem`=0, `Flags`=36700168, `FlagsEx`=0, `AllowableRaces`=6130900294268439629, `TimeAllowed`=0, `VerifiedBuild`=70862, `RewardFactionID1`=2826, `RewardFactionValue1`=1, `RewardFactionOverride1`=0, `RewardFactionCapIn1`=0 WHERE `ID`=95961 AND `VerifiedBuild`=-1;
DELETE FROM `quest_objectives` WHERE `QuestID`=95961;
DELETE FROM `quest_classic_money` WHERE `ID`=95961;
UPDATE `quest_classic_level` SET `QuestLevel`=38, `MinLevel`=36 WHERE `ID`=95961;
UPDATE `quest_template_classic_level` SET `LevelType`=2, `QuestLevel`=38 WHERE `ID`=95961;
UPDATE `quest_template` SET `QuestType`=2 WHERE `ID`=95961;

-- 96107 Feathers for Fletching (Riverglades, H): giver NPC 258031 (Questie); turn-in NPC 258031 (Questie)
DELETE FROM `quest_template` WHERE `ID`=96107;
INSERT INTO `quest_template` (`ID`,`QuestType`,`LogTitle`,`VerifiedBuild`) VALUES (96107,2,'Feathers for Fletching',-1);
DELETE FROM `quest_classic_level` WHERE `ID`=96107;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (96107,0,0,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=96107;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (96107,0,0);
DELETE FROM `quest_template_addon` WHERE `ID`=96107;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (96107,0,0,0);
DELETE FROM `quest_offer_reward` WHERE `ID`=96107;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (96107,'Well done, $n. Thank you.',-1);
DELETE FROM `creature_queststarter` WHERE `quest`=96107;
DELETE FROM `gameobject_queststarter` WHERE `quest`=96107;
DELETE FROM `creature_questender` WHERE `quest`=96107;
DELETE FROM `gameobject_questender` WHERE `quest`=96107;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (258031,96107,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (258031,96107,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (258031);
UPDATE `quest_template` SET `QuestDescription`='A hunter goes through more arrows in her lifetime than there are stars in the sky.\n\nPersonally, I make mine by hand. I only trust my own work.\n\nThe coarse feathers of the nearby buzzards make for excellent fletching. Would you aid me by collecting a few?', `LogDescription`='Bring $1oa Plains Buzzard Feathers to Neza Bloodsnout at Rog''mar.', `AreaDescription`='', `QuestCompletionLog`='', `LogTitle`='Feathers for Fletching', `QuestSortID`=16591, `QuestInfoID`=0, `SuggestedGroupNum`=0, `RewardNextQuest`=96108, `RewardXPDifficulty`=5, `RewardXPMultiplier`=1.0, `RewardMoneyDifficulty`=0, `RewardMoneyMultiplier`=1.0, `RewardSpell`=0, `StartItem`=0, `Flags`=36700168, `FlagsEx`=0, `AllowableRaces`=12261800583900083122, `TimeAllowed`=0, `VerifiedBuild`=70862 WHERE `ID`=96107 AND `VerifiedBuild`=-1;
DELETE FROM `quest_objectives` WHERE `QuestID`=96107;
DELETE FROM `quest_objectives` WHERE `ID` IN (472319);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (472319,96107,1,0,1,271793,6,0,0,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_classic_money` WHERE `ID`=96107;
UPDATE `quest_classic_level` SET `QuestLevel`=40, `MinLevel`=36 WHERE `ID`=96107;
UPDATE `quest_template_classic_level` SET `LevelType`=2, `QuestLevel`=40 WHERE `ID`=96107;
UPDATE `quest_template` SET `QuestType`=2 WHERE `ID`=96107;

-- Held back (102 quests, not in this file):
--   92844 Mountain of Bones: EG Link says it wants 426 Gently Cursed Bones; that needs checking in game before we copy it
--   92847 Corrupting Tears: item 254841: no source; item 254840: no source
--   92873 Proof of Concept: item 254842: no source
--   92894 Bone Raising: the quest is an event (escort, rescue, item use) that needs a script
--   93041 Facing the Truth: item 255945: no source
--   93058 Saving Our Own: kill/credit npc 254825 missing; the quest is an event (escort, rescue, item use) that needs a script
--   93066 Bristleback Retreat: kill/credit npc 253524 missing; kill/credit npc 253525 missing; kill/credit npc 253526 missing
--   93072 Long-Term Solutions: kill/credit npc 254283 missing
--   93122 Checking Things Out: objective type 19 needs a script
--   93125 Going Home: the quest is an event (escort, rescue, item use) that needs a script
--   93129 Threat of Starvation: item 256892: no source
--   93188 Return to Orgrul: comes after 93072, which is held
--   93787 Food for the Taking: item 262640: no source
--   93971 Farholde Scouting Report: start item 263930: no source
--   93976 The Goblin Trader: no turn-in known
--   94480 A Gift From Our Enemies: no giver known
--   94481 A Serving of Strength: no turn-in known
--   94556 Jom-jom's Escape: no giver known; the quest is an event (escort, rescue, item use) that needs a script
--   94615 Foal Play: item 267223: no source
--   94668 Bloodmaster's Ring: no giver known; no turn-in known
--   94669 Pay to Play: no giver known; no turn-in known
--   94670 Bloodmaster's Ring: no giver known; no turn-in known
--   94727 Digging for Answers: no turn-in known; kill/credit npc 260931 missing; the quest is an event (escort, rescue, item use) that needs a script
--   94733 Our Ogre Friend: no giver known
--   94734 A Boar's Delight: item 265808: no source; item 265806: no source
--   94737 A Treat for a Friend: comes after 94734, which is held
--   94739 Bloodmaster's Ring: no giver known; no turn-in known
--   94744 The Wayward Warrior: turn-in 258570 missing
--   94745 Carving a Path: giver 258570 missing; turn-in 258570 missing
--   94748 Break Their Chains: giver 258570 missing; turn-in 258570 missing; kill/credit npc 272737 missing; the quest is an event (escort, rescue, item use) that needs a script
--   94768 Bring Me His Head: giver 258570 missing; turn-in 258570 missing; item 265837: no source
--   94829 What We Lost: object 621813 missing
--   94843 From Seed to Sprout: object 621767 missing
--   94858 All That's Left: no turn-in known; item 266251: no source; item 266252: no source
--   94861 Helping Hands: no giver known; the quest is an event (escort, rescue, item use) that needs a script
--   94885 Field Marshal's Log: giver object 266427 missing
--   94887 Saving Marshal Ryan: no turn-in known; talk-to 257250 needs a script
--   94888 No Soldier Left Behind: kill/credit npc 272737 missing; the quest is an event (escort, rescue, item use) that needs a script
--   94890 Bring Me His Head: item 265837: no source
--   94891 News to Farholde: comes after 94890, which is held
--   94892 Dying Wish: no giver known
--   94893 Delivering Closure: turn-in 176553 missing
--   94894 The Twilight Threat: no giver known; turn-in 176553 missing
--   94907 A Small Favor: no giver known; kill/credit npc 271293 missing
--   94925 A Fur Trade: no giver known; no turn-in known; item 266888: no source
--   94926 Drop the Hammer: start item 266889: no source; no turn-in known; item 266889: no source
--   94927 Dunston Checks Out: no giver known
--   94962 Bloodmaster's Ring: no giver known; no turn-in known
--   94963 Biting the Hand: turn-in object 625582 missing
--   94964 Bloodmaster's Ring: no giver known; no turn-in known
--   94965 Bloodmaster's Ring: no turn-in known
--   94992 Rescue Roundup: no objective in the record and the same NPC at both ends: the real quest runs an event (escort or similar) that needs a script
--   95033 On the Other Side: no giver known
--   95063 Bloodmaster's Ring: no giver known
--   95066 The Rog'mar Situation: no giver known
--   95163 Drive Them Back: no giver known; no turn-in known; kill/credit npc 266883 missing
--   95165 See Ya, Scallywags!: object 629425 missing; object 629426 missing; object 629427 missing
--   95183 Step One: Rock: item 271892: no source
--   95267 The Monster Surfaces: item 268633: no source; the quest is an event (escort, rescue, item use) that needs a script
--   95278 It Is Time, Brother...: no giver known
--   95293 Smuggler's Special: turn-in 260390 missing
--   95295 I Know a Guy: no giver known
--   95330 Love Thy Neighbor: no giver known
--   95417 The Purple Ones: turn-in 260931 missing
--   95418 Spread the Word: giver 260931 missing
--   95420 Land of Giants: no turn-in known
--   95424 Might Makes Right: kill/credit npc 260968 missing; kill/credit npc 261497 missing
--   95425 Unworthy Champion: item 269257: no source
--   95426 Rog'mar Supplies: no giver known; no turn-in known; item 274975: no source
--   95427 Twilight No More!: giver 260931 missing; turn-in 260931 missing; item 265750: no source; item 265751: no source
--   95431 Rumblings of Discontent: no giver known; no turn-in known
--   95432 Rise Up!: no giver known; no turn-in known; kill/credit npc 260417 missing
--   95433 Death to the Executioner: item 269351: no source
--   95562 Cut Off the Heads: item 265750: no source; item 265751: no source
--   95601 The Purple Ones: giver 260931 missing; turn-in 260931 missing; kill/credit npc 261711 missing; kill/credit npc 262169 missing
--   95602 Sickening Lights: no giver known; no turn-in known; kill/credit npc 272603 missing
--   95613 Keeping the Light On: item 17028: no source
--   95615 In Memoriam: no giver known; object 642517 missing
--   95642 Abominable Magics: kill/credit npc 262006 missing
--   95643 Indiscriminate Purge: kill/credit npc 261711 missing; kill/credit npc 262169 missing
--   95882 WANTED: Scoutmaster Vargas: giver object 707249 missing
--   95935 Hail the Dragonslayer: start item 271612: no source; item 271612: no source
--   95936 Hail the Dragonslayer: start item 271613: no source; item 271613: no source
--   95951 Fetch 'n' Fletch: item 271630: no source; NPC 251320 (faction 12) is the wrong side for this H quest
--   95975 Her World: item 271823: no source
--   95982 Not My Circus: item 274971: no source
--   95983 A Feast for Buzzards: no giver known
--   95986 King of the Wild Plains: no giver known; item 281052: no source; the quest is an event (escort, rescue, item use) that needs a script
--   95992 WANTED: Ghostmaw: giver object 645164 missing
--   96015 A Miner Problem: kill/credit npc 260995 missing; kill/credit npc 260996 missing
--   96026 Step Two: More Rock: item 3241: no source
--   96082 Doctor's Orderly: no giver known
--   96083 Clerical Quarantine: no giver known
--   96084 Something's in the Water: item 281228: no source
--   96085 Upstream Contamination: kill/credit npc 263615 missing
--   96103 A Feast for Buzzards: no giver known
--   96104 Song of the Strider: comes after 96103, which is held
--   96105 King of the Wild Plains: item 281052: no source; the quest is an event (escort, rescue, item use) that needs a script
--   96108 Poison Tips: item 271776: no source
--   96109 A Hunter's Quiver: item 266888: no source
--   96126 An Ogre's Captive: the turn-in is Dunston (the captive), nobody has a spot or record for him; the log line names Gronok, the kill target
--   97884 Armory of Sunnyglade: item 278868: no source
