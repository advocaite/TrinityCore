-- Questie Forever corrections (Questie team, Questie/QuestieDB, master 80866960, 2026-11-05, GPLv3), hand fixes checked
-- against our world data.
-- A Light in the Darkness (98389, Aramis Hammerhand in Deathknell): Questie's traces show it only after The Damned (376).
-- We had no addon row, so it was offered before 376.
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (98389,376) ON DUPLICATE KEY UPDATE `PrevQuestID`=376;
-- Divine Grace (94773-94779) and Confounding Flash (94817, 94821-94826) are one-of-a-set quests. Only 94774 and 94817
-- exist in our data; give them their group now so the rest of each set joins it when added.
UPDATE `quest_template_addon` SET `ExclusiveGroup`=94773 WHERE `ID`=94774;
UPDATE `quest_template_addon` SET `ExclusiveGroup`=94817 WHERE `ID`=94817;
