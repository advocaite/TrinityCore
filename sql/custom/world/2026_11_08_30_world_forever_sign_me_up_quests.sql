-- Sign Me Up! (95816 Alliance, 95819 Horde): started by the Advertising License Application (270887/270888, sold by the Commerce
-- Authority, 2026_11_03_40). The quest hands out the Massive Pile of Forms (270896); using it makes the Massive Pile of Completed
-- Forms (270897, spell 1288322 in the client data), which goes to Elaine Compton (Redridge) or Jornah (The Barrens).
-- Built with the step 2 quest builder (all-forever-data-1107/step2_build_quests.py).
-- Built from Questie's Forever data (Questie team, Questie/QuestieDB, GPLv3,
-- https://github.com/Questie/QuestieDB, commit 5fba5d8 2026-11-07: src/corrections/Forever/ base data (from Wowhead's WoW Forever
-- quest pages), traces (player sessions) and fixes), checked against our own world: every giver, turn-in and kill target is a
-- spawned NPC or object we have, every objective item has a name in the client or our hotfixes.
-- PLACEHOLDER TEXT AND REWARDS: no open source has these quests' story text or rewards. Each quest uses its one-line goal as
-- log and story text, a short thank-you as reward text, XP and gold from the game's own level tables (QuestXP / QuestMoneyReward:
-- difficulty 5 for kill/collect quests, 1 for talk/deliver quests, no quest_classic_money row) and no item reward. Questie's
-- reputation rewards are kept. Every such quest has quest_template.VerifiedBuild = -1, so the real text and rewards (players'
-- game cache files, asked for on Discord 11-07) can replace exactly these rows later: WHERE VerifiedBuild = -1.
-- INFERRED: objective amounts are read from the goal line (else 1); where no turn-in is known the quest is handed in to its
-- giver; Questie race/class/zone names mapped to our masks (Skyborne = bits 32/33, as the client).
-- Safe to run again.

-- 95816 Sign Me Up! (level 60, required 1)
DELETE FROM `quest_template` WHERE `ID`=95816;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`) VALUES (95816,2,44,5,1,5,1,270896,0,4294967373,'Sign Me Up!','Bring the completed form to Elaine Compton in Redridge Mountains.','Bring the completed form to Elaine Compton in Redridge Mountains.','','Return to Elaine Compton.',-1);
DELETE FROM `quest_objectives` WHERE `QuestID`=95816;
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`Flags`,`Visible`,`Description`,`VerifiedBuild`) VALUES (9581600,95816,1,0,0,270897,1,0,1,NULL,-1);
DELETE FROM `quest_template_addon` WHERE `ID`=95816;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (95816,0,0,0);
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=19 AND `SourceEntry`=95816 AND `ConditionTypeOrReference`=8;
DELETE FROM `quest_request_items` WHERE `ID`=95816;
DELETE FROM `quest_offer_reward` WHERE `ID`=95816;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (95816,'Well done, $n. Thank you.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=95816;
DELETE FROM `quest_classic_level` WHERE `ID`=95816;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (95816,60,1,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=95816;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (95816,2,60);
DELETE FROM `creature_queststarter` WHERE `quest`=95816;
DELETE FROM `gameobject_queststarter` WHERE `quest`=95816;
DELETE FROM `creature_questender` WHERE `quest`=95816;
DELETE FROM `gameobject_questender` WHERE `quest`=95816;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (256391,95816,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (256391);
-- started by item 270888 (the items own StartQuestID)

-- 95819 Sign Me Up! (level 60, required 1)
DELETE FROM `quest_template` WHERE `ID`=95819;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`) VALUES (95819,2,17,5,1,5,1,270896,0,8589934770,'Sign Me Up!','Bring the completed form to Jornah in The Barrens.','Bring the completed form to Jornah in The Barrens.','','Return to Jornah.',-1);
DELETE FROM `quest_objectives` WHERE `QuestID`=95819;
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`Flags`,`Visible`,`Description`,`VerifiedBuild`) VALUES (9581900,95819,1,0,0,270897,1,0,1,NULL,-1);
DELETE FROM `quest_template_addon` WHERE `ID`=95819;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (95819,0,0,0);
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=19 AND `SourceEntry`=95819 AND `ConditionTypeOrReference`=8;
DELETE FROM `quest_request_items` WHERE `ID`=95819;
DELETE FROM `quest_offer_reward` WHERE `ID`=95819;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (95819,'Well done, $n. Thank you.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=95819;
DELETE FROM `quest_classic_level` WHERE `ID`=95819;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (95819,60,1,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=95819;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (95819,2,60);
DELETE FROM `creature_queststarter` WHERE `quest`=95819;
DELETE FROM `gameobject_queststarter` WHERE `quest`=95819;
DELETE FROM `creature_questender` WHERE `quest`=95819;
DELETE FROM `gameobject_questender` WHERE `quest`=95819;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (256388,95819,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (256388);
-- started by item 270887 (the items own StartQuestID)

