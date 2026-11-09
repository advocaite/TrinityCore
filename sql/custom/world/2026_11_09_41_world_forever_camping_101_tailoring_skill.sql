-- Camping 101: Tailoring 96057 (Elwynn camp) and 97925 had no quest_template_addon row, so they were offered to characters without
-- Tailoring and without the camp quest before them. Same as every other Camping 101 profession quest: Tailoring (197) 1 and the camp's
-- first quest (96608 / 95998, as the other quests of the same giver). Safe to run again.
INSERT INTO `quest_template_addon` (`ID`, `PrevQuestID`, `RequiredSkillID`, `RequiredSkillPoints`) VALUES
(96057, 96608, 197, 1),
(97925, 95998, 197, 1)
ON DUPLICATE KEY UPDATE `PrevQuestID` = VALUES(`PrevQuestID`), `RequiredSkillID` = VALUES(`RequiredSkillID`), `RequiredSkillPoints` = VALUES(`RequiredSkillPoints`);
