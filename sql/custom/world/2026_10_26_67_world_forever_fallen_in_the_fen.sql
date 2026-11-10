-- Classic 1.60 (Forever): Fallen in the Fen (95795, Alliance, level 31, requires 24, sharable, dungeon quest of Excavation Site: Wetlands).
-- From the Wowhead Forever page and list: ends at Dorin Songblade (956) in Lakeshire, 3150 XP, choice of Daewyn's Girdle (271769) or
-- Songblade Stabilizer (271768). The page shows no giver; the text ("You cover the dwarf's body with his cloak.") fits Daewyn Songblade
-- (262465), the dead dwarf inside the dungeon who already carries the quest giver flag: giver INFERRED. Paragraph breaks INFERRED (the
-- page text came without them).
DELETE FROM `quest_template` WHERE `ID`=95795;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestPackageID`,`ContentTuningID`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`RewardBonusMoney`,`RewardSpell`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`) VALUES
(95795,2,0,0,16732,81,0,0,6,1,0,1,0,0,0,8,4294967373,'Fallen in the Fen','Travel to Lakeshire and personally inform Dorin Songblade what became of his brother within the Excavation Sites.','<You cover the dwarf''s body with his cloak.>$B$BTravel to Lakeshire and personally inform Dorin Songblade what became of his brother within the Excavation Sites.','','',271769,1,271768,1);
DELETE FROM `quest_objectives` WHERE `QuestID`=95795;
DELETE FROM `quest_template_addon` WHERE `ID`=95795;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (95795,0,0,0);
DELETE FROM `quest_request_items` WHERE `ID`=95795;
DELETE FROM `quest_offer_reward` WHERE `ID`=95795;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (95795,'Ah no...$B$B<He buries his head in his hands and sobs.>$B$BI should ''ave gone with ''em. Much of what I was smithin'' was for ''im, was plannin'' a surprise but it wasn''t ready when he came by gettin'' things for his journey.$B$BNow that these are finished, I don''t have it in me to reforge or sell any of it.$B$BYou can have this. It''s the least I can do for you, bringing me this grim day.',0);
DELETE FROM `quest_classic_money` WHERE `ID`=95795;
DELETE FROM `quest_classic_level` WHERE `ID`=95795;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (95795,31,24,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=95795;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (95795,2,31);
DELETE FROM `creature_queststarter` WHERE `quest`=95795;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (262465,95795,0);
DELETE FROM `creature_questender` WHERE `quest`=95795;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (956,95795,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry`=956;
