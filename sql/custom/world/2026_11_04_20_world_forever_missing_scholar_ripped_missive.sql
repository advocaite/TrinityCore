-- The Missing Scholar (92727, Zephras, from Dondallion Whisperwind 253204, turned in at the Bloodstained Satchel 581822):
-- the sniff import gave it itself as previous quest, so nobody could ever take it and the chain on to 92849 stopped there.
-- Previous quest 92699 as in Questie's Forever quest fixes (Questie team, Questie/QuestieDB, GPLv3; id only).
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES (92727,92699) ON DUPLICATE KEY UPDATE `PrevQuestID` = 92699;

-- Ripped Missive (94490, starts from item 265476) and the quest item 265475 for quest 94488 both come from Commander Haalien
-- (253622, a named single target). The sniff import gave both a 66.67% chance (2 of 3 sniffed kills), so a third of kills left
-- the chain stuck. A named target drops its quest items every time (265475 still only while 94488 needs it).
UPDATE `creature_loot_template` SET `Chance` = 100 WHERE `Entry` = 253622 AND `Item` IN (265475,265476) AND `ItemType` = 0;
