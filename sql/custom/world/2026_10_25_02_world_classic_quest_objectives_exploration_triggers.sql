-- Exploration quests complete by entering an area trigger and have no quest_objectives row, so the client never ticks their
-- objective line. This adds one display-only objective per quest: Type 10 with ObjectID -1 credits whichever trigger completes it.
-- Idempotent (PK insert with ON DUPLICATE KEY UPDATE); rollback: DELETE FROM `quest_objectives` WHERE `VerifiedBuild` = 0 AND `ID` IN (ids below).

INSERT INTO `quest_objectives`
    (`ID`, `QuestID`, `Type`, `Order`, `StorageIndex`, `ObjectID`, `Amount`, `ConditionalAmount`,
     `Flags`, `Flags2`, `ProgressBarWeight`, `ParentObjectiveID`, `Visible`, `Description`, `VerifiedBuild`)
VALUES
    -- 62 The Fargodeep Mine
    (90000620,   62, 10, 0, 0, -1, 1, 0, 0, 0, 0, 0, 1, 'Scout through the Fargodeep Mine', 0),
    -- 76 The Jasperlode Mine
    (90000760,   76, 10, 0, 0, -1, 1, 0, 0, 0, 0, 0, 1, 'Scout through the Jasperlode Mine', 0),
    -- 201 Investigate the Camp
    (90002010,  201, 10, 0, 0, -1, 1, 0, 0, 0, 0, 0, 1, 'Locate the hunters'' camp', 0),
    -- 273 Resupplying the Excavation
    (90002730,  273, 10, 0, 0, -1, 1, 0, 0, 0, 0, 0, 1, 'Find Huldar, Miran, and Saean', 0),
    -- 578 The Stone of the Tides
    (90005780,  578, 10, 0, 0, -1, 1, 0, 0, 0, 0, 0, 1, 'Locate the haunted island', 0),
    -- 944 The Master's Glaive
    (90009440,  944, 10, 0, 0, -1, 1, 0, 0, 0, 0, 0, 1, 'Enter the Master''s Glaive', 0),
    -- 984 How Big a Threat?
    (90009840,  984, 10, 0, 0, -1, 1, 0, 0, 0, 0, 0, 1, 'Find a corrupt furbolg camp', 0),
    -- 1149 Test of Faith
    (90011490, 1149, 10, 0, 0, -1, 1, 0, 0, 0, 0, 0, 1, 'Leap from the planks overlooking Thousand Needles', 0),
    -- 1265 The Missing Diplomat
    (90012650, 1265, 10, 0, 0, -1, 1, 0, 0, 0, 0, 0, 1, 'Sentry Point explored', 0),
    -- 1448 In Search of The Temple
    (90014480, 1448, 10, 0, 0, -1, 1, 0, 0, 0, 0, 0, 1, 'Search for the Temple of Atal''Hakkar', 0),
    -- 1658 Crashing the Wickerman Festival
    (90016580, 1658, 10, 0, 0, -1, 1, 0, 0, 0, 0, 0, 1, 'Scout out the Forsaken''s Wickerman Festival.', 0),
    -- 1699 The Rethban Gauntlet
    (90016990, 1699, 10, 0, 0, -1, 1, 0, 0, 0, 0, 0, 1, 'Enter the Rethban Caverns', 0),
    -- 2240 The Hidden Chamber
    (90022400, 2240, 10, 0, 0, -1, 1, 0, 0, 0, 0, 0, 1, 'Explore the Hidden Chamber', 0),
    -- 2989 The Altar of Zul
    (90029890, 2989, 10, 0, 0, -1, 1, 0, 0, 0, 0, 0, 1, 'Search the Altar of Zul', 0),
    -- 4811 The Red Crystal
    (90048110, 4811, 10, 0, 0, -1, 1, 0, 0, 0, 0, 0, 1, 'Locate the large, red crystal on Darkshore''s eastern mountain range', 0),
    -- 4842 Strange Sources
    (90048420, 4842, 10, 0, 0, -1, 1, 0, 0, 0, 0, 0, 1, 'Discover Darkwhisper Gorge', 0),
    -- 6025 Unfinished Business
    (90060250, 6025, 10, 0, 0, -1, 1, 0, 0, 0, 0, 0, 1, 'Overlook Hearthglen from a high vantage point', 0),
    -- 8286 What Tomorrow Brings
    (90082860, 8286, 10, 0, 0, -1, 1, 0, 0, 0, 0, 0, 1, 'Discover the Brood of Nozdormu.', 0)
ON DUPLICATE KEY UPDATE
    `QuestID` = VALUES(`QuestID`),
    `Type` = VALUES(`Type`),
    `Order` = VALUES(`Order`),
    `StorageIndex` = VALUES(`StorageIndex`),
    `ObjectID` = VALUES(`ObjectID`),
    `Amount` = VALUES(`Amount`),
    `Visible` = VALUES(`Visible`),
    `Description` = VALUES(`Description`);
