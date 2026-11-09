-- Quest order fixes from the Questie project's hand-written Forever corrections (Questie team, Questie/QuestieDB,
-- src/corrections/Forever/foreverQuestFixes.lua, master 5fba5d86 / corrections b5a03ff, 2026-11-07, GPLv3).
-- Only chains still missing here after 2026_11_07_90 are set. Safe to run again.

-- 1) Ivar the Foul chain, Silverpine Forest: Return to Quinn (430) -> Wild Eyes (91920) -> Return to Quinn (Again) (91921, already linked)
--    -> Ivar the Foul (425). Wild Eyes and Ivar the Foul could be taken without the quest before.
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES
(91920,430),
(425,91921)
ON DUPLICATE KEY UPDATE `PrevQuestID`=VALUES(`PrevQuestID`);

-- 2) The Adventurer -> The Great Outdoors, one pair per starting area. Questie: each The Adventurer is a breadcrumb to its
--    The Great Outdoors (not a required quest), the seven The Great Outdoors are one-of-a-set, and so are the seven
--    The Adventurer. Here every The Great Outdoors needed its The Adventurer as PrevQuestID, so a player who skipped the
--    breadcrumb could never get it, and nothing stopped doing the same quest in a second starting area.
--    The Great Outdoors -> ExclusiveGroup 95998, no PrevQuestID; The Adventurer -> breadcrumb for its quest, ExclusiveGroup 96627.
--    The Adventurer keeps its own PrevQuestID.
UPDATE `quest_template_addon` SET `PrevQuestID`=0, `ExclusiveGroup`=95998 WHERE `ID`=95998 AND `PrevQuestID` IN (0,96627);
UPDATE `quest_template_addon` SET `PrevQuestID`=0, `ExclusiveGroup`=95998 WHERE `ID`=96101 AND `PrevQuestID` IN (0,96638);
UPDATE `quest_template_addon` SET `PrevQuestID`=0, `ExclusiveGroup`=95998 WHERE `ID`=96604 AND `PrevQuestID` IN (0,96652);
UPDATE `quest_template_addon` SET `PrevQuestID`=0, `ExclusiveGroup`=95998 WHERE `ID`=96605 AND `PrevQuestID` IN (0,96659);
UPDATE `quest_template_addon` SET `PrevQuestID`=0, `ExclusiveGroup`=95998 WHERE `ID`=96606 AND `PrevQuestID` IN (0,96630);
UPDATE `quest_template_addon` SET `PrevQuestID`=0, `ExclusiveGroup`=95998 WHERE `ID`=96607 AND `PrevQuestID` IN (0,96656);
UPDATE `quest_template_addon` SET `PrevQuestID`=0, `ExclusiveGroup`=95998 WHERE `ID`=96608 AND `PrevQuestID` IN (0,96628);
UPDATE `quest_template_addon` SET `BreadcrumbForQuestId`=95998, `ExclusiveGroup`=96627 WHERE `ID`=96627;
UPDATE `quest_template_addon` SET `BreadcrumbForQuestId`=96101, `ExclusiveGroup`=96627 WHERE `ID`=96638;
UPDATE `quest_template_addon` SET `BreadcrumbForQuestId`=96604, `ExclusiveGroup`=96627 WHERE `ID`=96652;
UPDATE `quest_template_addon` SET `BreadcrumbForQuestId`=96605, `ExclusiveGroup`=96627 WHERE `ID`=96659;
UPDATE `quest_template_addon` SET `BreadcrumbForQuestId`=96606, `ExclusiveGroup`=96627 WHERE `ID`=96630;
UPDATE `quest_template_addon` SET `BreadcrumbForQuestId`=96607, `ExclusiveGroup`=96627 WHERE `ID`=96656;
UPDATE `quest_template_addon` SET `BreadcrumbForQuestId`=96608, `ExclusiveGroup`=96627 WHERE `ID`=96628;
