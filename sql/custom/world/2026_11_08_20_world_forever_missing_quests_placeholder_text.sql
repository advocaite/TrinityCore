-- WoW Forever quests players do on Blizzard's servers that were missing from our world (groups A and B of the 11-07 count,
-- all-forever-data-1107/missing-quests-1107.tsv). Built from Questie's Forever data (Questie team, Questie/QuestieDB, GPLv3,
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

-- 92415 Remember That I Love You (level 22, required 15)
DELETE FROM `quest_template` WHERE `ID`=92415;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`RewardFactionID1`,`RewardFactionOverride1`,`VerifiedBuild`) VALUES (92415,2,16611,5,1,5,1,251522,0,4294967373,'Remember That I Love You','Bring the Blood-Stained Letter to Orphan Matron Nightingale in Stormwind City.','Bring the Blood-Stained Letter to Orphan Matron Nightingale in Stormwind City.','','Return to Orphan Matron Nightingale.',68,15000,-1);
DELETE FROM `quest_objectives` WHERE `QuestID`=92415;
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`Flags`,`Visible`,`Description`,`VerifiedBuild`) VALUES (9241500,92415,1,0,0,251522,1,0,1,NULL,-1);
DELETE FROM `quest_template_addon` WHERE `ID`=92415;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (92415,0,0,0);
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=19 AND `SourceEntry`=92415 AND `ConditionTypeOrReference`=8;
DELETE FROM `quest_request_items` WHERE `ID`=92415;
DELETE FROM `quest_offer_reward` WHERE `ID`=92415;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (92415,'Well done, $n. Thank you.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=92415;
DELETE FROM `quest_classic_level` WHERE `ID`=92415;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (92415,22,15,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=92415;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (92415,2,22);
DELETE FROM `creature_queststarter` WHERE `quest`=92415;
DELETE FROM `gameobject_queststarter` WHERE `quest`=92415;
DELETE FROM `creature_questender` WHERE `quest`=92415;
DELETE FROM `gameobject_questender` WHERE `quest`=92415;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (14450,92415,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (14450);
-- started by item 251270,251520 (the items own StartQuestID)

-- 92819 Destruction in Deadmines (level 18, required 9)
DELETE FROM `quest_template` WHERE `ID`=92819;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`RewardFactionID1`,`RewardFactionOverride1`,`VerifiedBuild`) VALUES (92819,2,40,5,1,5,1,0,0,4294967373,'Destruction in Deadmines','Use the detonator.','Use the detonator.','','Return to Alba Fairmoon.',72,5000,-1);
DELETE FROM `quest_objectives` WHERE `QuestID`=92819;
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`Flags`,`Visible`,`Description`,`VerifiedBuild`) VALUES (9281900,92819,2,0,0,581792,1,0,1,NULL,-1);
DELETE FROM `quest_template_addon` WHERE `ID`=92819;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (92819,0,0,92753);
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=19 AND `SourceEntry`=92819 AND `ConditionTypeOrReference`=8;
DELETE FROM `quest_request_items` WHERE `ID`=92819;
DELETE FROM `quest_offer_reward` WHERE `ID`=92819;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (92819,'Well done, $n. Thank you.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=92819;
DELETE FROM `quest_classic_level` WHERE `ID`=92819;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (92819,18,9,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=92819;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (92819,2,18);
DELETE FROM `creature_queststarter` WHERE `quest`=92819;
DELETE FROM `gameobject_queststarter` WHERE `quest`=92819;
DELETE FROM `creature_questender` WHERE `quest`=92819;
DELETE FROM `gameobject_questender` WHERE `quest`=92819;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (253279,92819,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (253279,92819,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (253279);

-- 94775 Divine Grace (level 10, required 10)
DELETE FROM `quest_template` WHERE `ID`=94775;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`) VALUES (94775,2,-262,1,1,1,1,0,0,1,'Divine Grace','Speak to High Priestess Laurena in Stormwind.','Speak to High Priestess Laurena in Stormwind.','','Return to High Priestess Laurena.',-1);
DELETE FROM `quest_objectives` WHERE `QuestID`=94775;
DELETE FROM `quest_template_addon` WHERE `ID`=94775;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (94775,0,16,0);
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=19 AND `SourceEntry`=94775 AND `ConditionTypeOrReference`=8;
DELETE FROM `quest_request_items` WHERE `ID`=94775;
DELETE FROM `quest_offer_reward` WHERE `ID`=94775;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (94775,'Well done, $n. Thank you.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=94775;
DELETE FROM `quest_classic_level` WHERE `ID`=94775;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (94775,10,10,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=94775;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (94775,2,10);
DELETE FROM `creature_queststarter` WHERE `quest`=94775;
DELETE FROM `gameobject_queststarter` WHERE `quest`=94775;
DELETE FROM `creature_questender` WHERE `quest`=94775;
DELETE FROM `gameobject_questender` WHERE `quest`=94775;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (3600,94775,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (376,94775,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (376,3600);

-- 94776 Divine Grace (level 10, required 10)
DELETE FROM `quest_template` WHERE `ID`=94776;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`) VALUES (94776,2,-262,1,1,1,1,0,0,1,'Divine Grace','Speak to High Priestess Laurena in Stormwind.','Speak to High Priestess Laurena in Stormwind.','','Return to High Priestess Laurena.',-1);
DELETE FROM `quest_objectives` WHERE `QuestID`=94776;
DELETE FROM `quest_template_addon` WHERE `ID`=94776;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (94776,0,16,0);
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=19 AND `SourceEntry`=94776 AND `ConditionTypeOrReference`=8;
DELETE FROM `quest_request_items` WHERE `ID`=94776;
DELETE FROM `quest_offer_reward` WHERE `ID`=94776;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (94776,'Well done, $n. Thank you.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=94776;
DELETE FROM `quest_classic_level` WHERE `ID`=94776;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (94776,10,10,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=94776;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (94776,2,10);
DELETE FROM `creature_queststarter` WHERE `quest`=94776;
DELETE FROM `gameobject_queststarter` WHERE `quest`=94776;
DELETE FROM `creature_questender` WHERE `quest`=94776;
DELETE FROM `gameobject_questender` WHERE `quest`=94776;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (1226,94776,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (376,94776,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (376,1226);

-- 94777 Divine Grace (level 10, required 10)
DELETE FROM `quest_template` WHERE `ID`=94777;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`) VALUES (94777,2,-262,1,1,1,1,0,0,1,'Divine Grace','Speak to High Priestess Laurena in Stormwind.','Speak to High Priestess Laurena in Stormwind.','','Return to High Priestess Laurena.',-1);
DELETE FROM `quest_objectives` WHERE `QuestID`=94777;
DELETE FROM `quest_template_addon` WHERE `ID`=94777;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (94777,0,16,0);
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=19 AND `SourceEntry`=94777 AND `ConditionTypeOrReference`=8;
DELETE FROM `quest_request_items` WHERE `ID`=94777;
DELETE FROM `quest_offer_reward` WHERE `ID`=94777;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (94777,'Well done, $n. Thank you.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=94777;
DELETE FROM `quest_classic_level` WHERE `ID`=94777;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (94777,10,10,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=94777;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (94777,2,10);
DELETE FROM `creature_queststarter` WHERE `quest`=94777;
DELETE FROM `gameobject_queststarter` WHERE `quest`=94777;
DELETE FROM `creature_questender` WHERE `quest`=94777;
DELETE FROM `gameobject_questender` WHERE `quest`=94777;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (11397,94777,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (376,94777,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (376,11397);

-- 95189 Crest of Lordaeron (level 22, required 15)
DELETE FROM `quest_template` WHERE `ID`=95189;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`) VALUES (95189,2,16611,5,1,5,1,268579,0,4294967373,'Crest of Lordaeron','Return the Crest of Lordaeron to Lady Dena Kennedy in Stormwind City.','Return the Crest of Lordaeron to Lady Dena Kennedy in Stormwind City.','','Return to Lady Dena Kennedy.',-1);
DELETE FROM `quest_objectives` WHERE `QuestID`=95189;
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`Flags`,`Visible`,`Description`,`VerifiedBuild`) VALUES (9518900,95189,1,0,0,268579,1,0,1,NULL,-1);
DELETE FROM `quest_template_addon` WHERE `ID`=95189;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (95189,0,0,0);
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=19 AND `SourceEntry`=95189 AND `ConditionTypeOrReference`=8;
DELETE FROM `quest_request_items` WHERE `ID`=95189;
DELETE FROM `quest_offer_reward` WHERE `ID`=95189;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (95189,'Well done, $n. Thank you.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=95189;
DELETE FROM `quest_classic_level` WHERE `ID`=95189;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (95189,22,15,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=95189;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (95189,2,22);
DELETE FROM `creature_queststarter` WHERE `quest`=95189;
DELETE FROM `gameobject_queststarter` WHERE `quest`=95189;
DELETE FROM `creature_questender` WHERE `quest`=95189;
DELETE FROM `gameobject_questender` WHERE `quest`=95189;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (15991,95189,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (15991);
-- started by item 268579 (the items own StartQuestID)

-- 98247 Shipping Label (level 60, required 10)
DELETE FROM `quest_template` WHERE `ID`=98247;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`RewardFactionID1`,`RewardFactionOverride1`,`VerifiedBuild`) VALUES (98247,2,44,5,1,5,1,0,0,4294967373,'Shipping Label','Deliver the Shipping Label to someone at the Azeroth Commerce Authority trade hub in Redridge Mountains.','Deliver the Shipping Label to someone at the Azeroth Commerce Authority trade hub in Redridge Mountains.','','Return to Marcy Baker.',2586,200000,-1);
DELETE FROM `quest_objectives` WHERE `QuestID`=98247;
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`Flags`,`Visible`,`Description`,`VerifiedBuild`) VALUES (9824700,98247,1,0,0,280179,1,0,1,NULL,-1);
DELETE FROM `quest_template_addon` WHERE `ID`=98247;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (98247,0,0,0);
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=19 AND `SourceEntry`=98247 AND `ConditionTypeOrReference`=8;
DELETE FROM `quest_request_items` WHERE `ID`=98247;
DELETE FROM `quest_offer_reward` WHERE `ID`=98247;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (98247,'Well done, $n. Thank you.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=98247;
DELETE FROM `quest_classic_level` WHERE `ID`=98247;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (98247,60,10,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=98247;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (98247,2,60);
DELETE FROM `creature_queststarter` WHERE `quest`=98247;
DELETE FROM `gameobject_queststarter` WHERE `quest`=98247;
DELETE FROM `creature_questender` WHERE `quest`=98247;
DELETE FROM `gameobject_questender` WHERE `quest`=98247;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (256390,98247,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (256390);
-- started by item 280179 (the items own StartQuestID)

-- 98248 Shipping Label (level 60, required 10)
DELETE FROM `quest_template` WHERE `ID`=98248;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`RewardFactionID1`,`RewardFactionOverride1`,`VerifiedBuild`) VALUES (98248,2,44,5,1,5,1,0,0,8589934770,'Shipping Label','Deliver the Shipping Label to someone at the Durotar Supply and Logistics trade hub near the Crossroads in the Barrens.','Deliver the Shipping Label to someone at the Durotar Supply and Logistics trade hub near the Crossroads in the Barrens.','','Return to Dokimi.',2587,200000,-1);
DELETE FROM `quest_objectives` WHERE `QuestID`=98248;
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`Flags`,`Visible`,`Description`,`VerifiedBuild`) VALUES (9824800,98248,1,0,0,280180,1,0,1,NULL,-1);
DELETE FROM `quest_template_addon` WHERE `ID`=98248;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (98248,0,0,0);
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=19 AND `SourceEntry`=98248 AND `ConditionTypeOrReference`=8;
DELETE FROM `quest_request_items` WHERE `ID`=98248;
DELETE FROM `quest_offer_reward` WHERE `ID`=98248;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (98248,'Well done, $n. Thank you.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=98248;
DELETE FROM `quest_classic_level` WHERE `ID`=98248;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (98248,60,10,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=98248;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (98248,2,60);
DELETE FROM `creature_queststarter` WHERE `quest`=98248;
DELETE FROM `gameobject_queststarter` WHERE `quest`=98248;
DELETE FROM `creature_questender` WHERE `quest`=98248;
DELETE FROM `gameobject_questender` WHERE `quest`=98248;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (256386,98248,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (256386);
-- started by item 280180 (the items own StartQuestID)

-- 94820 Contingency Plan (level 20, required 20)
DELETE FROM `quest_template` WHERE `ID`=94820;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`) VALUES (94820,2,-262,1,1,1,1,0,0,64,'Contingency Plan','Speak to High Priestess Mims in Ironforge.','Speak to High Priestess Mims in Ironforge.','','Return to High Priestess Mims.',-1);
DELETE FROM `quest_objectives` WHERE `QuestID`=94820;
DELETE FROM `quest_template_addon` WHERE `ID`=94820;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (94820,0,16,0);
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=19 AND `SourceEntry`=94820 AND `ConditionTypeOrReference`=8;
DELETE FROM `quest_request_items` WHERE `ID`=94820;
DELETE FROM `quest_offer_reward` WHERE `ID`=94820;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (94820,'Well done, $n. Thank you.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=94820;
DELETE FROM `quest_classic_level` WHERE `ID`=94820;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (94820,20,20,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=94820;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (94820,2,20);
DELETE FROM `creature_queststarter` WHERE `quest`=94820;
DELETE FROM `gameobject_queststarter` WHERE `quest`=94820;
DELETE FROM `creature_questender` WHERE `quest`=94820;
DELETE FROM `gameobject_questender` WHERE `quest`=94820;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (11401,94820,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (258785,94820,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (11401,258785);

-- 95772 Songblade Search (level 31, required 24)
DELETE FROM `quest_template` WHERE `ID`=95772;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`) VALUES (95772,2,16732,1,1,1,1,0,0,4294967373,'Songblade Search','Look for Dorin Songblade''s brother, Daewyn, in Whelgar''s Excavation Site.','Look for Dorin Songblade''s brother, Daewyn, in Whelgar''s Excavation Site.','','Return to Daewyn Songblade.',-1);
DELETE FROM `quest_objectives` WHERE `QuestID`=95772;
DELETE FROM `quest_template_addon` WHERE `ID`=95772;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (95772,0,0,0);
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=19 AND `SourceEntry`=95772 AND `ConditionTypeOrReference`=8;
DELETE FROM `quest_request_items` WHERE `ID`=95772;
DELETE FROM `quest_offer_reward` WHERE `ID`=95772;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (95772,'Well done, $n. Thank you.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=95772;
DELETE FROM `quest_classic_level` WHERE `ID`=95772;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (95772,31,24,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=95772;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (95772,2,31);
DELETE FROM `creature_queststarter` WHERE `quest`=95772;
DELETE FROM `gameobject_queststarter` WHERE `quest`=95772;
DELETE FROM `creature_questender` WHERE `quest`=95772;
DELETE FROM `gameobject_questender` WHERE `quest`=95772;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (956,95772,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (262465,95772,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (956,262465);

-- 96222 Khan Hratha (level 42, required 30) -- turn-in INFERRED = giver
DELETE FROM `quest_template` WHERE `ID`=96222;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`VerifiedBuild`) VALUES (96222,2,405,5,1,5,1,0,0,8589934770,'Khan Hratha','Bring the Maraudine Key Fragment to Umarak in the Desolace.','Bring the Maraudine Key Fragment to Umarak in the Desolace.','','Return to Umarak.',-1);
DELETE FROM `quest_objectives` WHERE `QuestID`=96222;
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`Flags`,`Visible`,`Description`,`VerifiedBuild`) VALUES (9622200,96222,1,0,0,6077,1,0,1,NULL,-1),(9622201,96222,1,1,1,6074,1,0,1,NULL,-1);
DELETE FROM `quest_template_addon` WHERE `ID`=96222;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (96222,0,0,0);
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=19 AND `SourceEntry`=96222 AND `ConditionTypeOrReference`=8;
DELETE FROM `quest_request_items` WHERE `ID`=96222;
DELETE FROM `quest_offer_reward` WHERE `ID`=96222;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (96222,'Well done, $n. Thank you.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=96222;
DELETE FROM `quest_classic_level` WHERE `ID`=96222;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (96222,42,30,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=96222;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (96222,2,42);
DELETE FROM `creature_queststarter` WHERE `quest`=96222;
DELETE FROM `gameobject_queststarter` WHERE `quest`=96222;
DELETE FROM `creature_questender` WHERE `quest`=96222;
DELETE FROM `gameobject_questender` WHERE `quest`=96222;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (253066,96222,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (253066,96222,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (253066);

-- 99192 A Donation of Silk (level 60, required 26) -- turn-in INFERRED = giver
DELETE FROM `quest_template` WHERE `ID`=99192;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`RewardFactionID1`,`RewardFactionOverride1`,`VerifiedBuild`) VALUES (99192,2,36,5,1,5,1,0,0,4294967373,'A Donation of Silk','Bring 60 Silk Cloth to Belanaa Windveil.','Bring 60 Silk Cloth to Belanaa Windveil.','','Return to Belanaa Windveil.',2779,15000,-1);
DELETE FROM `quest_objectives` WHERE `QuestID`=99192;
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`Flags`,`Visible`,`Description`,`VerifiedBuild`) VALUES (9919200,99192,1,0,0,4306,60,0,1,NULL,-1);
DELETE FROM `quest_template_addon` WHERE `ID`=99192;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (99192,0,0,0);
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=19 AND `SourceEntry`=99192 AND `ConditionTypeOrReference`=8;
DELETE FROM `quest_request_items` WHERE `ID`=99192;
DELETE FROM `quest_offer_reward` WHERE `ID`=99192;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (99192,'Well done, $n. Thank you.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=99192;
DELETE FROM `quest_classic_level` WHERE `ID`=99192;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (99192,60,26,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=99192;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (99192,2,60);
DELETE FROM `creature_queststarter` WHERE `quest`=99192;
DELETE FROM `gameobject_queststarter` WHERE `quest`=99192;
DELETE FROM `creature_questender` WHERE `quest`=99192;
DELETE FROM `gameobject_questender` WHERE `quest`=99192;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (276170,99192,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (276170,99192,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (276170);

-- 99197 A Donation of Silk (level 60, required 26) -- turn-in INFERRED = giver
DELETE FROM `quest_template` WHERE `ID`=99197;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestSortID`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`RewardFactionID1`,`RewardFactionOverride1`,`VerifiedBuild`) VALUES (99197,2,215,5,1,5,1,0,0,8589934770,'A Donation of Silk','Bring 60 Silk Cloth to Oura Stormspinner.','Bring 60 Silk Cloth to Oura Stormspinner.','','Return to Oura Stormspinner.',2778,15000,-1);
DELETE FROM `quest_objectives` WHERE `QuestID`=99197;
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`Flags`,`Visible`,`Description`,`VerifiedBuild`) VALUES (9919700,99197,1,0,0,4306,60,0,1,NULL,-1);
DELETE FROM `quest_template_addon` WHERE `ID`=99197;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (99197,0,0,0);
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=19 AND `SourceEntry`=99197 AND `ConditionTypeOrReference`=8;
DELETE FROM `quest_request_items` WHERE `ID`=99197;
DELETE FROM `quest_offer_reward` WHERE `ID`=99197;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (99197,'Well done, $n. Thank you.',-1);
DELETE FROM `quest_classic_money` WHERE `ID`=99197;
DELETE FROM `quest_classic_level` WHERE `ID`=99197;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (99197,60,26,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=99197;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (99197,2,60);
DELETE FROM `creature_queststarter` WHERE `quest`=99197;
DELETE FROM `gameobject_queststarter` WHERE `quest`=99197;
DELETE FROM `creature_questender` WHERE `quest`=99197;
DELETE FROM `gameobject_questender` WHERE `quest`=99197;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (276171,99197,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (276171,99197,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (276171);

-- Remember That I Love You: the Duskwood part (95161) comes after the Stormwind part (92415), as Questie has it.
UPDATE `quest_template_addon` SET `PrevQuestID` = 92415 WHERE `ID` = 95161;

