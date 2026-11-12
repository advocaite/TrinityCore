-- Exploration objectives of the live quests (Discord 2026-11-09, zeroyka: "The Missing Diplomat" (1265), the step in the Sentry Point
-- tower never counts, so the quest can't be turned in to Archmage Tervosh).
--
-- On Forever the "explore" step of 28 quests is a quest objective of type 10 (area trigger) with the trigger the client sends,
-- for 1265 objective 385516 on trigger 1667 ("Sentry Point explored"). Our row for 1265 kept only the old 1.12 link
-- (areatrigger_involvedrelation 302 -> 1265) and no objective, so the client had no step to tick and the quest never completed.
-- Data from Efficient Games (https://efficient.games), licensed under CC BY 4.0: the quest objectives of the 70862 client cache;
-- every trigger exists in this client's AreaTrigger data.
--
-- A row goes in only where the quest exists and has no area trigger objective, no objective with that ID and nothing on that
-- storage index yet, so a quest the database already handles keeps its own row. The server maps each trigger to its quest from
-- these rows (ObjectMgr::LoadQuestAreaTriggers), the old involvedrelation rows stay as they are.

INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`)
SELECT 385521,25,10,1,1,2926,1,0,0,0,0,0,1,'Scout the gazebo on Mystral Lake that overlooks the nearby Alliance outpost.',0 FROM DUAL
WHERE EXISTS (SELECT 1 FROM `quest_template` WHERE `ID` = 25)
  AND NOT EXISTS (SELECT 1 FROM `quest_objectives` WHERE `ID` = 385521 OR (`QuestID` = 25 AND (`Type` = 10 OR `StorageIndex` = 1))); -- Stonetalon Standstill
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`)
SELECT 385473,76,10,0,0,87,1,0,0,0,0,0,1,'Scout through the Jasperlode Mine',0 FROM DUAL
WHERE EXISTS (SELECT 1 FROM `quest_template` WHERE `ID` = 76)
  AND NOT EXISTS (SELECT 1 FROM `quest_objectives` WHERE `ID` = 385473 OR (`QuestID` = 76 AND (`Type` = 10 OR `StorageIndex` = 0))); -- The Jasperlode Mine
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`)
SELECT 385476,201,10,0,0,98,1,0,0,0,0,0,1,'Locate the hunters\' camp',0 FROM DUAL
WHERE EXISTS (SELECT 1 FROM `quest_template` WHERE `ID` = 201)
  AND NOT EXISTS (SELECT 1 FROM `quest_objectives` WHERE `ID` = 385476 OR (`QuestID` = 201 AND (`Type` = 10 OR `StorageIndex` = 0))); -- Investigate the Camp
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`)
SELECT 385475,287,10,1,1,97,1,0,0,0,0,0,1,'Fully explore Frostmane Hold',0 FROM DUAL
WHERE EXISTS (SELECT 1 FROM `quest_template` WHERE `ID` = 287)
  AND NOT EXISTS (SELECT 1 FROM `quest_objectives` WHERE `ID` = 385475 OR (`QuestID` = 287 AND (`Type` = 10 OR `StorageIndex` = 1))); -- Frostmane Hold
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`)
SELECT 385479,437,10,1,1,173,1,0,0,0,0,0,1,'Enter the Dead Fields',0 FROM DUAL
WHERE EXISTS (SELECT 1 FROM `quest_template` WHERE `ID` = 437)
  AND NOT EXISTS (SELECT 1 FROM `quest_objectives` WHERE `ID` = 385479 OR (`QuestID` = 437 AND (`Type` = 10 OR `StorageIndex` = 1))); -- The Dead Fields
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`)
SELECT 385480,455,10,2,2,175,1,0,0,0,0,0,1,'Traverse Dun Algaz',0 FROM DUAL
WHERE EXISTS (SELECT 1 FROM `quest_template` WHERE `ID` = 455)
  AND NOT EXISTS (SELECT 1 FROM `quest_objectives` WHERE `ID` = 385480 OR (`QuestID` = 455 AND (`Type` = 10 OR `StorageIndex` = 2))); -- The Algaz Gauntlet
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`)
SELECT 385481,503,10,1,1,178,1,0,0,0,0,0,1,'Find where Gol\'dir is being held',0 FROM DUAL
WHERE EXISTS (SELECT 1 FROM `quest_template` WHERE `ID` = 503)
  AND NOT EXISTS (SELECT 1 FROM `quest_objectives` WHERE `ID` = 385481 OR (`QuestID` = 503 AND (`Type` = 10 OR `StorageIndex` = 1))); -- Gol'dir
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`)
SELECT 385482,578,10,0,0,196,1,0,0,0,0,0,1,'Locate the haunted island',0 FROM DUAL
WHERE EXISTS (SELECT 1 FROM `quest_template` WHERE `ID` = 578)
  AND NOT EXISTS (SELECT 1 FROM `quest_objectives` WHERE `ID` = 385482 OR (`QuestID` = 578 AND (`Type` = 10 OR `StorageIndex` = 0))); -- The Stone of the Tides
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`)
SELECT 385484,870,10,0,0,216,1,0,0,0,0,0,1,'Explore the waters of the Forgotten Pools',0 FROM DUAL
WHERE EXISTS (SELECT 1 FROM `quest_template` WHERE `ID` = 870)
  AND NOT EXISTS (SELECT 1 FROM `quest_objectives` WHERE `ID` = 385484 OR (`QuestID` = 870 AND (`Type` = 10 OR `StorageIndex` = 0))); -- The Forgotten Pools
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`)
SELECT 385489,944,10,0,0,223,0,0,0,0,0,0,1,'Enter the Master\'s Glaive',0 FROM DUAL
WHERE EXISTS (SELECT 1 FROM `quest_template` WHERE `ID` = 944)
  AND NOT EXISTS (SELECT 1 FROM `quest_objectives` WHERE `ID` = 385489 OR (`QuestID` = 944 AND (`Type` = 10 OR `StorageIndex` = 0))); -- The Master's Glaive
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`)
SELECT 385491,1149,10,0,0,246,1,0,0,0,0,0,1,'',0 FROM DUAL
WHERE EXISTS (SELECT 1 FROM `quest_template` WHERE `ID` = 1149)
  AND NOT EXISTS (SELECT 1 FROM `quest_objectives` WHERE `ID` = 385491 OR (`QuestID` = 1149 AND (`Type` = 10 OR `StorageIndex` = 0))); -- Test of Faith
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`)
SELECT 385516,1265,10,0,0,1667,1,0,0,0,0,0,1,'Sentry Point explored',0 FROM DUAL
WHERE EXISTS (SELECT 1 FROM `quest_template` WHERE `ID` = 1265)
  AND NOT EXISTS (SELECT 1 FROM `quest_objectives` WHERE `ID` = 385516 OR (`QuestID` = 1265 AND (`Type` = 10 OR `StorageIndex` = 0))); -- The Missing Diplomat
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`)
SELECT 385492,1448,10,0,0,362,1,0,0,0,0,0,1,'Search for the Temple of Atal\'Hakkar',0 FROM DUAL
WHERE EXISTS (SELECT 1 FROM `quest_template` WHERE `ID` = 1448)
  AND NOT EXISTS (SELECT 1 FROM `quest_objectives` WHERE `ID` = 385492 OR (`QuestID` = 1448 AND (`Type` = 10 OR `StorageIndex` = 0))); -- In Search of The Temple
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`)
SELECT 385504,1658,10,0,0,3991,1,0,0,0,0,0,1,'Scout out the Forsaken\'s Wickerman Festival.',0 FROM DUAL
WHERE EXISTS (SELECT 1 FROM `quest_template` WHERE `ID` = 1658)
  AND NOT EXISTS (SELECT 1 FROM `quest_objectives` WHERE `ID` = 385504 OR (`QuestID` = 1658 AND (`Type` = 10 OR `StorageIndex` = 0))); -- Crashing the Wickerman Festival
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`)
SELECT 385494,1699,10,0,0,482,1,0,0,0,0,0,1,'Enter the Rethban Caverns',0 FROM DUAL
WHERE EXISTS (SELECT 1 FROM `quest_template` WHERE `ID` = 1699)
  AND NOT EXISTS (SELECT 1 FROM `quest_objectives` WHERE `ID` = 385494 OR (`QuestID` = 1699 AND (`Type` = 10 OR `StorageIndex` = 0))); -- The Rethban Gauntlet
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`)
SELECT 385495,1719,10,1,1,522,1,0,0,0,0,0,1,'Step on the grate to begin the Affray',0 FROM DUAL
WHERE EXISTS (SELECT 1 FROM `quest_template` WHERE `ID` = 1719)
  AND NOT EXISTS (SELECT 1 FROM `quest_objectives` WHERE `ID` = 385495 OR (`QuestID` = 1719 AND (`Type` = 10 OR `StorageIndex` = 1))); -- The Affray
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`)
SELECT 385513,2240,10,0,0,822,1,0,0,0,0,0,1,'Explore the Hidden Chamber',0 FROM DUAL
WHERE EXISTS (SELECT 1 FROM `quest_template` WHERE `ID` = 2240)
  AND NOT EXISTS (SELECT 1 FROM `quest_objectives` WHERE `ID` = 385513 OR (`QuestID` = 2240 AND (`Type` = 10 OR `StorageIndex` = 0))); -- The Hidden Chamber
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`)
SELECT 385519,4811,10,0,0,2486,1,0,0,0,0,0,1,'Locate the large, red crystal on Darkshore\'s eastern mountain range',0 FROM DUAL
WHERE EXISTS (SELECT 1 FROM `quest_template` WHERE `ID` = 4811)
  AND NOT EXISTS (SELECT 1 FROM `quest_objectives` WHERE `ID` = 385519 OR (`QuestID` = 4811 AND (`Type` = 10 OR `StorageIndex` = 0))); -- The Red Crystal
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`)
SELECT 385518,4842,10,0,0,2327,1,0,0,0,0,0,1,'Discover Darkwhisper Gorge',0 FROM DUAL
WHERE EXISTS (SELECT 1 FROM `quest_template` WHERE `ID` = 4842)
  AND NOT EXISTS (SELECT 1 FROM `quest_objectives` WHERE `ID` = 385518 OR (`QuestID` = 4842 AND (`Type` = 10 OR `StorageIndex` = 0))); -- Strange Sources
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`)
SELECT 385517,5156,10,2,2,2206,1,0,0,0,0,0,1,'Explore the craters in Shatter Scar Vale',0 FROM DUAL
WHERE EXISTS (SELECT 1 FROM `quest_template` WHERE `ID` = 5156)
  AND NOT EXISTS (SELECT 1 FROM `quest_objectives` WHERE `ID` = 385517 OR (`QuestID` = 5156 AND (`Type` = 10 OR `StorageIndex` = 2))); -- Verifying the Corruption
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`)
SELECT 385496,6025,10,0,0,3366,1,0,0,0,0,0,1,'Overlook Hearthglen from a high vantage point',0 FROM DUAL
WHERE EXISTS (SELECT 1 FROM `quest_template` WHERE `ID` = 6025)
  AND NOT EXISTS (SELECT 1 FROM `quest_objectives` WHERE `ID` = 385496 OR (`QuestID` = 6025 AND (`Type` = 10 OR `StorageIndex` = 0))); -- Unfinished Business
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`)
SELECT 385520,6185,10,3,3,2726,1,0,0,0,0,0,1,'The Blightcaller Uncovered',0 FROM DUAL
WHERE EXISTS (SELECT 1 FROM `quest_template` WHERE `ID` = 6185)
  AND NOT EXISTS (SELECT 1 FROM `quest_objectives` WHERE `ID` = 385520 OR (`QuestID` = 6185 AND (`Type` = 10 OR `StorageIndex` = 3))); -- The Eastern Plagues
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`)
SELECT 385522,6421,10,1,1,2946,1,0,0,0,0,0,1,'Investigate Cave in Boulderslide Ravine',0 FROM DUAL
WHERE EXISTS (SELECT 1 FROM `quest_template` WHERE `ID` = 6421)
  AND NOT EXISTS (SELECT 1 FROM `quest_objectives` WHERE `ID` = 385522 OR (`QuestID` = 6421 AND (`Type` = 10 OR `StorageIndex` = 1))); -- Boulderslide Ravine
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`)
SELECT 385506,9261,10,1,1,4098,1,0,0,0,0,0,1,'Investigate a circle',0 FROM DUAL
WHERE EXISTS (SELECT 1 FROM `quest_template` WHERE `ID` = 9261)
  AND NOT EXISTS (SELECT 1 FROM `quest_objectives` WHERE `ID` = 385506 OR (`QuestID` = 9261 AND (`Type` = 10 OR `StorageIndex` = 1))); -- Investigate the Scourge of Ironforge
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`)
SELECT 385510,9262,10,1,1,4104,1,0,0,0,0,0,1,'Investigate a circle',0 FROM DUAL
WHERE EXISTS (SELECT 1 FROM `quest_template` WHERE `ID` = 9262)
  AND NOT EXISTS (SELECT 1 FROM `quest_objectives` WHERE `ID` = 385510 OR (`QuestID` = 9262 AND (`Type` = 10 OR `StorageIndex` = 1))); -- Investigate the Scourge of Darnassus
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`)
SELECT 385508,9263,10,1,1,4101,1,0,0,0,0,0,1,'Investigate a circle',0 FROM DUAL
WHERE EXISTS (SELECT 1 FROM `quest_template` WHERE `ID` = 9263)
  AND NOT EXISTS (SELECT 1 FROM `quest_objectives` WHERE `ID` = 385508 OR (`QuestID` = 9263 AND (`Type` = 10 OR `StorageIndex` = 1))); -- Investigate the Scourge of Orgrimmar
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`)
SELECT 385509,9264,10,1,1,4103,1,0,0,0,0,0,1,'Investigate a circle',0 FROM DUAL
WHERE EXISTS (SELECT 1 FROM `quest_template` WHERE `ID` = 9264)
  AND NOT EXISTS (SELECT 1 FROM `quest_objectives` WHERE `ID` = 385509 OR (`QuestID` = 9264 AND (`Type` = 10 OR `StorageIndex` = 1))); -- Investigate the Scourge of Thunder Bluff
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`)
SELECT 385507,9265,10,1,1,4100,1,0,0,0,0,0,1,'Investigate a circle',0 FROM DUAL
WHERE EXISTS (SELECT 1 FROM `quest_template` WHERE `ID` = 9265)
  AND NOT EXISTS (SELECT 1 FROM `quest_objectives` WHERE `ID` = 385507 OR (`QuestID` = 9265 AND (`Type` = 10 OR `StorageIndex` = 1))); -- Investigate the Scourge of the Undercity
