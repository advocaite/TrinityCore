-- Blizzard hotfix 6 November 2026: "The Sleeper Has Awakened": "Players also start with the Horn of Awakening."
-- Accepting the quest (5321, Kerlonian Evershade, Darkshore) now hands over the Horn of Awakening (13536), the item that wakes
-- Kerlonian when he falls asleep on the way. It goes away again if the quest is dropped. Kerlonian's Chest still holds one as
-- before. The other half of the hotfix (he falls asleep less often) is in classic_darkshore.cpp.
-- Safe to run again.
UPDATE `quest_template` SET `StartItem`=13536 WHERE `ID`=5321;
INSERT INTO `quest_template_addon` (`ID`,`ProvidedItemCount`) VALUES (5321,1) ON DUPLICATE KEY UPDATE `ProvidedItemCount`=1;
