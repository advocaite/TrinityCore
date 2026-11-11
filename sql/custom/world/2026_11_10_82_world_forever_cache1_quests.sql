-- WoW Forever: 3 quests from a player's live game cache (Cache 1.zip, enUS build 70862, sent 2026-11-10) that our world lacked.
-- Text, levels, objectives, money and rewards are the cache's own record. Givers and turn-ins: Questie's Forever data (QuestieDB
-- aca0da1, GPLv3), see each quest. The cache has no reward text, so that is a short placeholder (VerifiedBuild -1 marks it).
-- Held (not built): 94894 The Twilight Threat and 92858 WANTED: Ohk'zi (giver unknown, Overlord Kilgar 256881 and Ohk'zi 222233
-- have no template), 94983/94984 (Krol'dok Stronghold, not built yet), 97536 Gnomish Garbage (needs a script for dumping the crate),
-- 97046 Your First Mechanostrider (giver unknown, 3 of 4 mechanostriders not spawned), 97188 Learning to Ride (its objective spell
-- 231900 is not in our client spell table).
-- Safe to run again.

-- 96081 Wound Dressings: giver and turn-in Brother Lowley (Questie Forever data)
DELETE FROM `quest_template` WHERE `ID`=96081;
INSERT INTO `quest_template` (`ID`,`QuestType`,`LogTitle`,`VerifiedBuild`) VALUES (96081,2,'Wound Dressings',-1);
DELETE FROM `quest_classic_level` WHERE `ID`=96081;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (96081,0,0,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=96081;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (96081,0,0);
DELETE FROM `quest_template_addon` WHERE `ID`=96081;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (96081,0,0,0);
DELETE FROM `quest_offer_reward` WHERE `ID`=96081;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (96081,'Well done, $n. Thank you.',-1);
DELETE FROM `creature_queststarter` WHERE `quest`=96081;
DELETE FROM `creature_questender` WHERE `quest`=96081;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (263613,96081,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (263613,96081,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (263613,263613);
UPDATE `quest_template` SET `QuestDescription`='If there is one thing a fighting force always needs, it''s bandages. Unfortunately, Farholde has no reliable way to manufacture cloth. Thus, we depend on imports from the goblins or, if we are lucky, what we can take back from the orcs.\n\nCurrently, our supply of quality cloth is running low. I will have to start making bandages of old shirts before long. If you have the means to aid us, you would be saving many lives.', `LogDescription`='Gather 20 Mageweave Cloth for Brother Lowley at Farholde Keep.', `AreaDescription`='', `QuestCompletionLog`='', `LogTitle`='Wound Dressings', `QuestSortID`=16591, `QuestInfoID`=0, `SuggestedGroupNum`=0, `RewardNextQuest`=0, `RewardXPDifficulty`=3, `RewardXPMultiplier`=1.0, `RewardMoneyDifficulty`=0, `RewardMoneyMultiplier`=1.0, `RewardSpell`=0, `StartItem`=0, `Flags`=8, `FlagsEx`=0, `AllowableRaces`=6130900294268439629, `TimeAllowed`=0, `VerifiedBuild`=70862, `RewardItem1`=0, `RewardAmount1`=0, `ItemDrop1`=0, `ItemDropQuantity1`=0, `RewardItem2`=0, `RewardAmount2`=0, `ItemDrop2`=0, `ItemDropQuantity2`=0, `RewardItem3`=0, `RewardAmount3`=0, `ItemDrop3`=0, `ItemDropQuantity3`=0, `RewardItem4`=0, `RewardAmount4`=0, `ItemDrop4`=0, `ItemDropQuantity4`=0, `RewardChoiceItemID1`=0, `RewardChoiceItemQuantity1`=0, `RewardChoiceItemDisplayID1`=0, `RewardChoiceItemID2`=0, `RewardChoiceItemQuantity2`=0, `RewardChoiceItemDisplayID2`=0, `RewardChoiceItemID3`=0, `RewardChoiceItemQuantity3`=0, `RewardChoiceItemDisplayID3`=0, `RewardChoiceItemID4`=0, `RewardChoiceItemQuantity4`=0, `RewardChoiceItemDisplayID4`=0, `RewardChoiceItemID5`=0, `RewardChoiceItemQuantity5`=0, `RewardChoiceItemDisplayID5`=0, `RewardChoiceItemID6`=0, `RewardChoiceItemQuantity6`=0, `RewardChoiceItemDisplayID6`=0, `RewardFactionID1`=2826, `RewardFactionValue1`=5, `RewardFactionOverride1`=0, `RewardFactionCapIn1`=7, `RewardFactionID2`=0, `RewardFactionValue2`=0, `RewardFactionOverride2`=0, `RewardFactionCapIn2`=7, `RewardFactionID3`=0, `RewardFactionValue3`=0, `RewardFactionOverride3`=0, `RewardFactionCapIn3`=7, `RewardFactionID4`=0, `RewardFactionValue4`=0, `RewardFactionOverride4`=0, `RewardFactionCapIn4`=7, `RewardFactionID5`=0, `RewardFactionValue5`=0, `RewardFactionOverride5`=0, `RewardFactionCapIn5`=7 WHERE `ID`=96081 AND `VerifiedBuild`=-1;
DELETE FROM `quest_objectives` WHERE `QuestID`=96081;
DELETE FROM `quest_objectives` WHERE `ID` IN (472118);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (472118,96081,1,0,0,4338,20,0,32,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_classic_money` WHERE `ID`=96081;
UPDATE `quest_classic_level` SET `QuestLevel`=40, `MinLevel`=36 WHERE `ID`=96081;
UPDATE `quest_template_classic_level` SET `LevelType`=2, `QuestLevel`=40 WHERE `ID`=96081;
UPDATE `quest_template` SET `QuestType`=2 WHERE `ID`=96081;

-- 95988 Goes Down Smooth: turn-in Innkeeper Zizplink (quest text); giver ESTIMATED as Zizplink too, since he hands out his own Grocery List (the quest gives it on accept); nobody has the real giver yet
DELETE FROM `quest_template` WHERE `ID`=95988;
INSERT INTO `quest_template` (`ID`,`QuestType`,`LogTitle`,`VerifiedBuild`) VALUES (95988,2,'Goes Down Smooth',-1);
DELETE FROM `quest_classic_level` WHERE `ID`=95988;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (95988,0,0,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=95988;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (95988,0,0);
DELETE FROM `quest_template_addon` WHERE `ID`=95988;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (95988,0,0,0);
DELETE FROM `quest_offer_reward` WHERE `ID`=95988;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (95988,'Well done, $n. Thank you.',-1);
DELETE FROM `creature_queststarter` WHERE `quest`=95988;
DELETE FROM `creature_questender` WHERE `quest`=95988;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (255894,95988,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (255894,95988,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (255894,255894);
UPDATE `quest_template` SET `QuestDescription`='Talked to Gray, didja? Ball of laughs, that guy. Last thing he needs is you gettin'' him another drink. This is an inn not a bar. He''s lucky I haven''t thrown him out.\n\nYou tryin'' to take him with you, be my guest. Though I don''t think he''s in any condition to go anywhere like that. Not without a little goblin pick-me-up.\n\nWhy don''t ya gather some ingredients for me from this list while I prepare your little friend his ''drink.'' We''ll see if he still wants booze after he''s had my specialty cocktail.', `LogDescription`='Gather $1oa Crawler Spittle, Fresh Chum, Goblin Degreaser, and Expired Poison for Innkeeper Zizplink in Powderfuse Port.', `AreaDescription`='', `QuestCompletionLog`='', `LogTitle`='Goes Down Smooth', `QuestSortID`=16591, `QuestInfoID`=0, `SuggestedGroupNum`=0, `RewardNextQuest`=95957, `RewardXPDifficulty`=5, `RewardXPMultiplier`=1.0, `RewardMoneyDifficulty`=0, `RewardMoneyMultiplier`=1.0, `RewardSpell`=0, `StartItem`=276101, `Flags`=36700160, `FlagsEx`=0, `AllowableRaces`=6130900294268439629, `TimeAllowed`=0, `VerifiedBuild`=70862, `RewardItem1`=274935, `RewardAmount1`=5, `ItemDrop1`=0, `ItemDropQuantity1`=0, `RewardItem2`=0, `RewardAmount2`=0, `ItemDrop2`=0, `ItemDropQuantity2`=0, `RewardItem3`=0, `RewardAmount3`=0, `ItemDrop3`=0, `ItemDropQuantity3`=0, `RewardItem4`=0, `RewardAmount4`=0, `ItemDrop4`=0, `ItemDropQuantity4`=0, `RewardChoiceItemID1`=0, `RewardChoiceItemQuantity1`=0, `RewardChoiceItemDisplayID1`=0, `RewardChoiceItemID2`=0, `RewardChoiceItemQuantity2`=0, `RewardChoiceItemDisplayID2`=0, `RewardChoiceItemID3`=0, `RewardChoiceItemQuantity3`=0, `RewardChoiceItemDisplayID3`=0, `RewardChoiceItemID4`=0, `RewardChoiceItemQuantity4`=0, `RewardChoiceItemDisplayID4`=0, `RewardChoiceItemID5`=0, `RewardChoiceItemQuantity5`=0, `RewardChoiceItemDisplayID5`=0, `RewardChoiceItemID6`=0, `RewardChoiceItemQuantity6`=0, `RewardChoiceItemDisplayID6`=0, `RewardFactionID1`=0, `RewardFactionValue1`=0, `RewardFactionOverride1`=0, `RewardFactionCapIn1`=7, `RewardFactionID2`=0, `RewardFactionValue2`=0, `RewardFactionOverride2`=0, `RewardFactionCapIn2`=7, `RewardFactionID3`=0, `RewardFactionValue3`=0, `RewardFactionOverride3`=0, `RewardFactionCapIn3`=7, `RewardFactionID4`=0, `RewardFactionValue4`=0, `RewardFactionOverride4`=0, `RewardFactionCapIn4`=7, `RewardFactionID5`=0, `RewardFactionValue5`=0, `RewardFactionOverride5`=0, `RewardFactionCapIn5`=7 WHERE `ID`=95988 AND `VerifiedBuild`=-1;
DELETE FROM `quest_objectives` WHERE `QuestID`=95988;
DELETE FROM `quest_objectives` WHERE `ID` IN (471769,471770,471771,471772);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (471769,95988,1,0,0,271650,3,0,32,0,0.0,0,1,NULL,70862),(471770,95988,1,1,2,271657,1,0,32,0,0.0,0,1,NULL,70862),(471771,95988,1,2,3,271653,1,0,32,0,0.0,0,1,NULL,70862),(471772,95988,1,3,4,271654,1,0,32,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_classic_money` WHERE `ID`=95988;
UPDATE `quest_classic_level` SET `QuestLevel`=38, `MinLevel`=36 WHERE `ID`=95988;
UPDATE `quest_template_classic_level` SET `LevelType`=2, `QuestLevel`=38 WHERE `ID`=95988;
UPDATE `quest_template` SET `QuestType`=2 WHERE `ID`=95988;

-- 97530 Dunemaul Dues: giver and turn-in Security Chief Bilgewhizzle (Questie Forever data)
DELETE FROM `quest_template` WHERE `ID`=97530;
INSERT INTO `quest_template` (`ID`,`QuestType`,`LogTitle`,`VerifiedBuild`) VALUES (97530,2,'Dunemaul Dues',-1);
DELETE FROM `quest_classic_level` WHERE `ID`=97530;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (97530,0,0,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=97530;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (97530,0,0);
DELETE FROM `quest_template_addon` WHERE `ID`=97530;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (97530,0,0,0);
DELETE FROM `quest_offer_reward` WHERE `ID`=97530;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (97530,'Well done, $n. Thank you.',-1);
DELETE FROM `creature_queststarter` WHERE `quest`=97530;
DELETE FROM `creature_questender` WHERE `quest`=97530;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (7882,97530,0);
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (7882,97530,0);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (7882,7882);
UPDATE `quest_template` SET `QuestDescription`='A merchant caravan took a wrong turn through the desert and got jumped by the Dunemaul ogres. Only reason they made it out alive is ''cause they tossed some cargo overboard to distract the brutes.\n\nMust be this merchant''s got friends with the nobs runnin'' ships and zeppelins, ''cause now they want ME pickin'' up what got lost.\n\nI''m stuck here working. But if YOU see a nice shiny bangle while you''re beatin'' on ogres, I''ll make it worth ya while.', `LogDescription`='Security Chief Bilgewhizzle of Steamwheedle Port in Tanaris wants you to recover a particularly shiny bangle bracelet lost near the Dunemaul ogre compound.', `AreaDescription`='', `QuestCompletionLog`='', `LogTitle`='Dunemaul Dues', `QuestSortID`=440, `QuestInfoID`=0, `SuggestedGroupNum`=0, `RewardNextQuest`=0, `RewardXPDifficulty`=5, `RewardXPMultiplier`=1.0, `RewardMoneyDifficulty`=3, `RewardMoneyMultiplier`=1.0, `RewardSpell`=0, `StartItem`=0, `Flags`=8, `FlagsEx`=0, `AllowableRaces`=18446744073709551615, `TimeAllowed`=0, `VerifiedBuild`=70862, `RewardItem1`=281282, `RewardAmount1`=1, `ItemDrop1`=0, `ItemDropQuantity1`=0, `RewardItem2`=0, `RewardAmount2`=0, `ItemDrop2`=0, `ItemDropQuantity2`=0, `RewardItem3`=0, `RewardAmount3`=0, `ItemDrop3`=0, `ItemDropQuantity3`=0, `RewardItem4`=0, `RewardAmount4`=0, `ItemDrop4`=0, `ItemDropQuantity4`=0, `RewardChoiceItemID1`=0, `RewardChoiceItemQuantity1`=0, `RewardChoiceItemDisplayID1`=0, `RewardChoiceItemID2`=0, `RewardChoiceItemQuantity2`=0, `RewardChoiceItemDisplayID2`=0, `RewardChoiceItemID3`=0, `RewardChoiceItemQuantity3`=0, `RewardChoiceItemDisplayID3`=0, `RewardChoiceItemID4`=0, `RewardChoiceItemQuantity4`=0, `RewardChoiceItemDisplayID4`=0, `RewardChoiceItemID5`=0, `RewardChoiceItemQuantity5`=0, `RewardChoiceItemDisplayID5`=0, `RewardChoiceItemID6`=0, `RewardChoiceItemQuantity6`=0, `RewardChoiceItemDisplayID6`=0, `RewardFactionID1`=369, `RewardFactionValue1`=5, `RewardFactionOverride1`=0, `RewardFactionCapIn1`=7, `RewardFactionID2`=0, `RewardFactionValue2`=0, `RewardFactionOverride2`=0, `RewardFactionCapIn2`=7, `RewardFactionID3`=0, `RewardFactionValue3`=0, `RewardFactionOverride3`=0, `RewardFactionCapIn3`=7, `RewardFactionID4`=0, `RewardFactionValue4`=0, `RewardFactionOverride4`=0, `RewardFactionCapIn4`=7, `RewardFactionID5`=0, `RewardFactionValue5`=0, `RewardFactionOverride5`=0, `RewardFactionCapIn5`=7 WHERE `ID`=97530 AND `VerifiedBuild`=-1;
DELETE FROM `quest_objectives` WHERE `QuestID`=97530;
DELETE FROM `quest_objectives` WHERE `ID` IN (476052);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (476052,97530,1,0,4,278186,1,0,32,0,0.0,0,1,NULL,70862);
DELETE FROM `quest_classic_money` WHERE `ID`=97530;
INSERT INTO `quest_classic_money` (`ID`,`RewardMoney`,`RewardMoneyMaxLevel`) VALUES (97530,3500,0);
UPDATE `quest_classic_level` SET `QuestLevel`=47, `MinLevel`=40 WHERE `ID`=97530;
UPDATE `quest_template_classic_level` SET `LevelType`=2, `QuestLevel`=47 WHERE `ID`=97530;
UPDATE `quest_template` SET `QuestType`=2 WHERE `ID`=97530;

-- Item sources:
-- 95988: Crawler Spittle 271650 drops from Shore Crawler 256807 (Questie Forever data), quest-only, chance 35 ESTIMATED like the
--        crawler's other quest item. Fresh Chum, Goblin Degreaser and Expired Poison are already sold in Powderfuse Port.
DELETE FROM `creature_loot_template` WHERE `Entry`=256807 AND `ItemType`=0 AND `Item`=271650;
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(256807,0,271650,35,1,1,0,1,1,'Shore Crawler - quest 95988 Crawler Spittle (Questie Forever drop, chance ESTIMATED)');
-- 97530: Brilliantly Polished Bangle 278186 already drops from the Dunemaul ogres 5471-5475, but at 0.003-0.012% (all kills counted,
--        not only questing ones), so the one bangle the quest needs would take thousands of kills. Now quest-only at 5% ESTIMATED.
UPDATE `creature_loot_template` SET `Chance`=5, `QuestRequired`=1 WHERE `Entry` IN (5471,5472,5473,5474,5475) AND `ItemType`=0 AND `Item`=278186;

