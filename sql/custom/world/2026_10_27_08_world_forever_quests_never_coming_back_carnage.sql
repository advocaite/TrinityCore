-- Two more WoW Forever quests from the Wowhead Forever pages (same page tool and rules as 2026_10_26_39): Never Coming Back
-- (98251, Va'xug Firefure, Ashenvale) and Carnage (98283, Rethiel the Greenwarden, Wetlands, after Blisters on The Land).
-- Their givers and objectives are all in our world now: kill Shadowhorn Stag (3817) and Ashenvale Bear (3809); Malignant
-- Rotclaw Flesh (280331) and Carnage's Head (280330), which already drop from 271334 and 271336 (sniffed loot).
-- Every other Forever quest still missing has a page that names an NPC, object or item source we don't have, or only a
-- summary without objective ids (no page HTML), so it waits for data.

-- 98251 Never Coming Back (level 22, required 18) -- Wowhead Forever page
DELETE FROM `quest_template` WHERE `ID`=98251;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestPackageID`,`ContentTuningID`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`RewardBonusMoney`,`RewardSpell`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`RewardFactionID1`,`RewardFactionOverride1`) VALUES (98251,2,0,0,1638,0,0,0,5,1,0,1,0,0,0,8,8589934770,'Never Coming Back','Kill 6 Shadowhorn Stag and 6 Ashenvale Bear and return to Va''xug Firefure northwest of Fallen Sky Lake.','Gaaah!$B$BI''m so angry. They said Ashenvale was easy to follow. Easy for who? The beasts? The birds?$B$BOne moment I was gathering an herb, the next I''m trapped in this ravine. Do I truly have to circle half the forest to reach where I stood just moments ago?$B$BWhen I''m like this, my spells grow volatile and behave... differently.$B$BCould you thin the wildlife nearby? Even their stupid hooves grate on me. I despise this cursed forest.','','',81,1000);
DELETE FROM `quest_objectives` WHERE `QuestID`=98251;
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (9825100,98251,0,0,0,3817,6,0,0,0,0,0,1,'',0);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (9825101,98251,0,1,1,3809,6,0,0,0,0,0,1,'',0);
DELETE FROM `quest_template_addon` WHERE `ID`=98251;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (98251,0,0,0);
DELETE FROM `quest_request_items` WHERE `ID`=98251;
INSERT INTO `quest_request_items` (`ID`,`CompletionText`,`VerifiedBuild`) VALUES (98251,'Once I get out of this place, I''m never returning.',0);
DELETE FROM `quest_offer_reward` WHERE `ID`=98251;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (98251,'Ha! That''s right, you beasts!$B$BThanks. That does seem to already have helped.',0);
DELETE FROM `quest_classic_money` WHERE `ID`=98251;
INSERT INTO `quest_classic_money` (`ID`,`RewardMoney`,`RewardMoneyMaxLevel`) VALUES (98251,700,0);
DELETE FROM `quest_classic_level` WHERE `ID`=98251;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (98251,22,18,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=98251;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (98251,2,22);
DELETE FROM `creature_queststarter` WHERE `quest`=98251;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (267214,98251,0);
DELETE FROM `creature_questender` WHERE `quest`=98251;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (267214,98251,0);
DELETE FROM `gameobject_queststarter` WHERE `quest`=98251;
DELETE FROM `gameobject_questender` WHERE `quest`=98251;
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry`=267214;

-- 98283 Carnage (level 30, required 22) -- Wowhead Forever page
DELETE FROM `quest_template` WHERE `ID`=98283;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestPackageID`,`ContentTuningID`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`RewardBonusMoney`,`RewardSpell`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`RewardChoiceItemID1`,`RewardChoiceItemQuantity1`,`RewardChoiceItemID2`,`RewardChoiceItemQuantity2`) VALUES (98283,2,0,0,11,1,0,0,6,1,0,1,0,0,0,8,4294967373,'Carnage','Bring Carnage''s Head to Rethiel the Greenwarden in The Green Belt.','The natural order of this land is breaking. Once it was that the Razormaw raptors kept the land from overgrowth. They now live in fear of their alpha, named only for the carnage it leaves behind.$B$BI fear the beast has been driven mad by the sickness that we have yet to fully excise.$B$BIt will respond to the malignant flesh of the diseased bears below Raptor Ridge, to the northeast. Leave their entrails in its cave lair.$B$BCarnage will come. Bring me its head.','','',281309,1,281310,1);
DELETE FROM `quest_objectives` WHERE `QuestID`=98283;
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (9828300,98283,1,0,0,280331,1,0,0,0,0,0,1,'Malignant Rotclaw Flesh',0);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (9828301,98283,1,1,1,280330,1,0,0,0,0,0,1,'Carnage''s Head',0);
DELETE FROM `quest_template_addon` WHERE `ID`=98283;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (98283,0,0,275);
DELETE FROM `quest_request_items` WHERE `ID`=98283;
INSERT INTO `quest_request_items` (`ID`,`CompletionText`,`VerifiedBuild`) VALUES (98283,'The Rotclaw came down from their homes in Loch Modan, now driven mad.$B$BSuch a waste of life''s boon.',0);
DELETE FROM `quest_offer_reward` WHERE `ID`=98283;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (98283,'Carnage''s death will bring balance to his pack. Perhaps it will buy enough time for the greater ecosystem they support.$B$BWe shall see, but I thank you for the effort regardless.',0);
DELETE FROM `quest_classic_money` WHERE `ID`=98283;
DELETE FROM `quest_classic_level` WHERE `ID`=98283;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (98283,30,22,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=98283;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (98283,2,30);
DELETE FROM `creature_queststarter` WHERE `quest`=98283;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (1244,98283,0);
DELETE FROM `creature_questender` WHERE `quest`=98283;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (1244,98283,0);
DELETE FROM `gameobject_queststarter` WHERE `quest`=98283;
DELETE FROM `gameobject_questender` WHERE `quest`=98283;
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry`=1244;
