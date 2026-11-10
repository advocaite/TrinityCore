-- Classic 1.60 (Forever): Forced Disarmament (98293, Alliance, elite, level 30, requires 22), Wetlands. From the Wowhead Forever page and list:
-- starts at the object Dragonmaw Armaments (672330) at 58.8, 45.2, ends at Captain Stoutfist (2104) in Menethil Keep, 2450 XP and
-- 100 Wildhammer Clan reputation. No objective on the page (take it from the armaments, hand it in).
-- The object is in no data we have (not in the 70532 GameObjects.db2, not in the other Forever server): template INFERRED (a quest giver object
-- with the vanilla Weapon Crate model 335). Position from the map pin: zone 58.8, 45.2 -> (-3393.1, -2819.7), fitted on four
-- spawned Wetlands NPCs (within 5 yd); it stands in the Dragonmaw Reclaimer camp. Height INFERRED from the Reclaimer next to it.
DELETE FROM `gameobject_template` WHERE `entry`=672330;
INSERT INTO `gameobject_template` (`entry`,`type`,`displayId`,`name`,`IconName`,`castBarCaption`,`unk1`,`size`,`ContentTuningId`,`RequiredLevel`,`AIName`,`ScriptName`,`VerifiedBuild`) VALUES
(672330,2,335,'Dragonmaw Armaments','','','',1,0,0,'','',0);
DELETE FROM `gameobject_template_addon` WHERE `entry`=672330;
DELETE FROM `gameobject` WHERE `guid` BETWEEN 67233000 AND 67233099;
INSERT INTO `gameobject` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`,`ScriptName`,`StringId`,`VerifiedBuild`) VALUES
(67233000,672330,0,0,0,'0',0,0,0,-1,-3393.1,-2819.7,12.6,0,0,0,0,1,120,255,1,'',NULL,0);

DELETE FROM `quest_template` WHERE `ID`=98293;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestPackageID`,`ContentTuningID`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`RewardBonusMoney`,`RewardSpell`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`RewardFactionID1`,`RewardFactionOverride1`) VALUES
(98293,2,0,0,11,1,0,0,5,1,0,1,0,0,0,0,4294967373,'Forced Disarmament','Captain Stoutfist in Menethil Keep would surely be interested in these.','The Dragonmaw are stockpiling all manner of weaponry. It would be wise to relieve them of these before more soldiers can be armed.$B$BCaptain Stoutfist in Menethil Keep would surely be interested in these.','','',471,10000);
DELETE FROM `quest_objectives` WHERE `QuestID`=98293;
DELETE FROM `quest_template_addon` WHERE `ID`=98293;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (98293,0,0,0);
DELETE FROM `quest_request_items` WHERE `ID`=98293;
INSERT INTO `quest_request_items` (`ID`,`CompletionText`,`VerifiedBuild`) VALUES (98293,'What''s that you have there, $C?',0);
DELETE FROM `quest_offer_reward` WHERE `ID`=98293;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (98293,'Well done! Very well done!$B$BWe can definitely put these to use in replenishing our men. Half of these are from our own soldiers to begin with.$B$BFewer weapons in the hands of the enemy and more in ours is my kind of math, $C.',0);
DELETE FROM `quest_classic_money` WHERE `ID`=98293;
DELETE FROM `quest_classic_level` WHERE `ID`=98293;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (98293,30,22,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=98293;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (98293,2,30);
DELETE FROM `gameobject_queststarter` WHERE `quest`=98293;
INSERT INTO `gameobject_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (672330,98293,0);
DELETE FROM `creature_questender` WHERE `quest`=98293;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (2104,98293,0);
