-- Quest 5727 "Hidden Enemies" (Thrall, Orgrimmar): the only map blob sat on Thrall's hall, the quest giver and
-- turn-in, so the log had no marker at the NPC the objective actually needs. Its objective is to take the
-- Lieutenant's Insignia to Neeru Fireblade in the Cleft of Shadow (creature 3216, spawn 1801 / -4375 / -17,
-- the same spot quest 5729's blob uses), so the blob is moved there and given the classic shape
-- (`ObjectiveIndex` -1, `Flags` 1) that renders on the 1.60.1 client for an objective-less quest.
UPDATE `quest_poi`
SET `Flags` = 1, `ObjectiveIndex` = -1, `QuestObjectiveID` = 0, `QuestObjectID` = 0
WHERE `QuestID` = 5727 AND `Idx1` = 0;

UPDATE `quest_poi_points`
SET `X` = 1801, `Y` = -4375, `Z` = -17
WHERE `QuestID` = 5727 AND `Idx1` = 0 AND `Idx2` = 0;

-- rollback:
-- UPDATE `quest_poi` SET `Flags` = 3 WHERE `QuestID` = 5727 AND `Idx1` = 0;
-- UPDATE `quest_poi_points` SET `X` = 1929, `Y` = -4159, `Z` = 0 WHERE `QuestID` = 5727 AND `Idx1` = 0 AND `Idx2` = 0;
