-- Quest 5727 "Hidden Enemies" (Thrall, Orgrimmar): the only map blob sat on Thrall's hall, the quest giver and turn-in, so the
-- log had no marker at the NPC the step needs. The quest has no objectives: the player takes the Lieutenant's Insignia to
-- Neeru Fireblade in the Cleft of Shadow (creature 3216, spawn 1801 / -4375 / -17, the spot quest 5729's blob uses) and comes
-- back. The blob moves there with the objective-less shape (`ObjectiveIndex` -1, `Flags` 1) the 1.60.1 client draws.
-- From Duy Hoang (dqhoang), advocaite/TrinityCore PR #263. Safe to run again.
UPDATE `quest_poi` SET `Flags`=1, `ObjectiveIndex`=-1, `QuestObjectiveID`=0, `QuestObjectID`=0 WHERE `QuestID`=5727 AND `Idx1`=0;
UPDATE `quest_poi_points` SET `X`=1801, `Y`=-4375, `Z`=-17 WHERE `QuestID`=5727 AND `Idx1`=0 AND `Idx2`=0;
