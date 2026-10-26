-- Quest 870 "The Forgotten Pools" has no quest_objectives row, so its area-trigger completion never ticks the log line.
-- Display-only: the trigger still completes the quest. Idempotent.

INSERT INTO `quest_objectives`
    (`ID`, `QuestID`, `Type`, `Order`, `StorageIndex`, `ObjectID`, `Amount`, `ConditionalAmount`,
     `Flags`, `Flags2`, `ProgressBarWeight`, `ParentObjectiveID`, `Visible`, `Description`, `VerifiedBuild`)
VALUES
    (90008700, 870, 10, 0, 0, 216, 1, 0, 0, 0, 0, 0, 1,
     'Explore the waters of the Forgotten Pools', 0)
ON DUPLICATE KEY UPDATE
    `QuestID` = VALUES(`QuestID`),
    `Type` = VALUES(`Type`),
    `Order` = VALUES(`Order`),
    `StorageIndex` = VALUES(`StorageIndex`),
    `ObjectID` = VALUES(`ObjectID`),
    `Amount` = VALUES(`Amount`),
    `Visible` = VALUES(`Visible`),
    `Description` = VALUES(`Description`);

-- Rollback: DELETE FROM `quest_objectives` WHERE `ID` = 90008700 AND `QuestID` = 870;
