-- WoW Forever live quest data from EG Link (Efficient Games), JIVESCORP/eg-link-output-wowf commit 164bd1e2, folders 1.60.1.70862
-- and 1.60.1.70888 (70888 wins where both have the quest). Data from Efficient Games (https://efficient.games), licensed under
-- CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/). Changed: compared with our world data and written as TrinityCore rows.
-- The game's quest records as players' clients received them from the live realm. VerifiedBuild = the EG folder's build.
-- Only what the 12 Nov file (2026_11_12_14, EG Link commit 3067c69) did not already take, and only quests we already have.
-- Safe to run more than once.

-- 1. Objectives live has and we lack, 11 quests: the reputation each Paragons of Power quest needs with the Zandalar Tribe
--    (8056, 8057, 8060, 8068, 8069, 8075), the Arathor / Defiler care packages (8261, 8264), Timbermaw for Winterfall Ritual
--    Totem (8471), Argent Dawn for The Dread Citadel - Naxxramas (9121), and the runic circle to look at in Investigate the
--    Scourge of Stormwind (9260; its area trigger link was already there). Ours let anyone do them at any standing.
DELETE FROM `quest_objectives` WHERE `ID` IN (382901,383093,382897,383120,383184,383158,384280,383723,384105,382517,385505);
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES
(382901,8056,6,1,1,270,3000,0,0,0,0,0,1,NULL,70888),
(383093,8057,6,1,1,270,3000,0,0,0,0,0,1,NULL,70888),
(382897,8060,6,1,1,270,3000,0,0,0,0,0,1,NULL,70888),
(383120,8068,6,1,1,270,9000,0,0,0,0,0,1,NULL,70888),
(383184,8069,6,1,1,270,21000,0,0,0,0,0,1,NULL,70888),
(383158,8075,6,1,1,270,21000,0,0,0,0,0,1,NULL,70888),
(384280,8261,6,0,0,509,3000,0,0,0,0,0,1,NULL,70888),
(383723,8264,6,0,0,510,3000,0,0,0,0,0,1,NULL,70888),
(384105,8471,6,1,1,576,0,0,0,0,0,0,1,NULL,70888),
(382517,9121,6,3,3,529,9000,0,0,0,0,0,1,NULL,70888),
(385505,9260,10,1,1,4092,1,0,0,0,0,0,1,NULL,70888);

-- 2. Follow-up quest offered at turn-in (RewardNextQuest), 3 quests. The Captain's Cutlass had another follow-up in ours.
UPDATE `quest_template` SET `RewardNextQuest` = 8554 WHERE `ID` = 615; -- The Captain's Cutlass (ours 618)
UPDATE `quest_template` SET `RewardNextQuest` = 3791 WHERE `ID` = 3787; -- Jonespyre's Request
UPDATE `quest_template` SET `RewardNextQuest` = 3791 WHERE `ID` = 3788; -- Jonespyre's Request

-- 3. Spell cast on the player at turn-in (RewardSpell), 2 quests.
UPDATE `quest_template` SET `RewardSpell` = 364167 WHERE `ID` = 5305; -- Sweet Serenity (ours 17044)
UPDATE `quest_template` SET `RewardSpell` = 366430 WHERE `ID` = 7603; -- Kroshius' Infernal Core (ours 1413)

-- 4. Reward items as live lists them: 5 quests with fixed items, 6 with a choice. Mostly the new Forever reward items
--    (27xxxx) next to the old ones. Where the live record shows no items at all we keep ours (see the report).
UPDATE `quest_template` SET `RewardItem1` = 270275, `RewardAmount1` = 1, `RewardItem2` = 270276, `RewardAmount2` = 1, `RewardItem3` = 0, `RewardAmount3` = 0, `RewardItem4` = 0, `RewardAmount4` = 0 WHERE `ID` = 2994; -- Saving Sharpbeak
UPDATE `quest_template` SET `RewardItem1` = 19035, `RewardAmount1` = 1, `RewardItem2` = 287074, `RewardAmount2` = 1, `RewardItem3` = 0, `RewardAmount3` = 0, `RewardItem4` = 0, `RewardAmount4` = 0 WHERE `ID` = 7840; -- Lard Lost His Lunch
UPDATE `quest_template` SET `RewardItem1` = 20086, `RewardAmount1` = 1, `RewardItem2` = 0, `RewardAmount2` = 0, `RewardItem3` = 0, `RewardAmount3` = 0, `RewardItem4` = 0, `RewardAmount4` = 0 WHERE `ID` = 8249; -- Junkboxes Needed
UPDATE `quest_template` SET `RewardItem1` = 287076, `RewardAmount1` = 1, `RewardItem2` = 0, `RewardAmount2` = 0, `RewardItem3` = 0, `RewardAmount3` = 0, `RewardItem4` = 0, `RewardAmount4` = 0 WHERE `ID` = 8282; -- Noggle's Lost Satchel
UPDATE `quest_template` SET `RewardItem1` = 20504, `RewardAmount1` = 1, `RewardItem2` = 20505, `RewardAmount2` = 1, `RewardItem3` = 20512, `RewardAmount3` = 1, `RewardItem4` = 20620, `RewardAmount4` = 1 WHERE `ID` = 8418; -- Forging the Mightstone
UPDATE `quest_template` SET `RewardChoiceItemID1` = 11469, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 270100, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 0, `RewardChoiceItemQuantity3` = 0, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 614; -- The Captain's Chest
UPDATE `quest_template` SET `RewardChoiceItemID1` = 9645, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 9646, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270101, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 836; -- Rescue OOX-09/HL!
UPDATE `quest_template` SET `RewardChoiceItemID1` = 9651, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 9652, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270112, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 2994; -- Saving Sharpbeak
UPDATE `quest_template` SET `RewardChoiceItemID1` = 11910, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 11911, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 11913, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 270122, `RewardChoiceItemQuantity4` = 1, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 4491; -- A Little Help From My Friends
UPDATE `quest_template` SET `RewardChoiceItemID1` = 18491, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 270124, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 270125, `RewardChoiceItemQuantity3` = 1, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 7488; -- Lethtendris's Web
UPDATE `quest_template` SET `RewardChoiceItemID1` = 20133, `RewardChoiceItemQuantity1` = 1, `RewardChoiceItemID2` = 20134, `RewardChoiceItemQuantity2` = 1, `RewardChoiceItemID3` = 0, `RewardChoiceItemQuantity3` = 0, `RewardChoiceItemID4` = 0, `RewardChoiceItemQuantity4` = 0, `RewardChoiceItemID5` = 0, `RewardChoiceItemQuantity5` = 0, `RewardChoiceItemID6` = 0, `RewardChoiceItemQuantity6` = 0 WHERE `ID` = 8258; -- The Darkreaver Menace

-- 5. Reward display spells (quest_reward_display_spell) on 9 more quests, shown in the reward window. All of them have a
--    RewardSpell, so nothing new is cast at turn-in.
DELETE FROM `quest_reward_display_spell` WHERE `QuestID` IN (2772,2773,5283,5301,5305,5640,7603,8313,8798);
INSERT INTO `quest_reward_display_spell` (`QuestID`,`Idx`,`SpellID`,`PlayerConditionID`,`Type`,`VerifiedBuild`) VALUES
(2772,0,9979,0,2,70888),
(2773,0,9972,0,2,70888),
(5283,0,9788,0,3,70888),
(5301,0,9788,0,3,70888),
(5305,0,17040,0,3,70888),
(5640,0,13908,0,3,70888),
(7603,0,1122,0,3,70888),
(8313,0,24801,0,2,70888),
(8798,0,26011,0,2,70888);
