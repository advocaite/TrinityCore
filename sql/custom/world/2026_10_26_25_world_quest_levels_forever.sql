-- Quest level / required level that differ from the Forever 70532 quest list (Wowhead Forever, 10-25).
-- Forever live design wins. Only live quests; entries where the list shows required level 0/1 (no data) are left out.
-- Battleground quests move to Forever's brackets (quest level 25/35/45/55), which also brings their XP in line.
DROP TEMPORARY TABLE IF EXISTS `tmp_quest_level`;
CREATE TEMPORARY TABLE `tmp_quest_level` (`ID` INT UNSIGNED PRIMARY KEY, `QuestLevel` SMALLINT, `MinLevel` TINYINT UNSIGNED);
INSERT INTO `tmp_quest_level` (`ID`,`QuestLevel`,`MinLevel`) VALUES
(299,27,22), -- Uncovering the Past, was 28/25
(472,30,25), -- Fall of Dun Modr, was 25/25
(544,33,30), -- Prison Break In, was 34/30
(556,33,30), -- Stone Tokens, was 32/30
(557,33,30), -- Bracers of Binding, was 34/30
(631,31,26), -- The Thandol Span, was 31/28
(632,31,26), -- The Thandol Span, was 31/28
(633,31,26), -- The Thandol Span, was 31/28
(932,8,4), -- Twisted Hatred, was 7/4
(952,17,6), -- Grove of the Ancients, was 11/6
(971,23,18), -- Knowledge in the Deeps, was 23/10
(1132,30,30), -- Fiora Longears, was 20/18
(1152,30,26), -- Test of Lore, was 30/25
(1154,30,26), -- Test of Lore, was 30/25
(1159,30,26), -- Test of Lore, was 30/25
(1160,36,26), -- Test of Lore, was 36/25
(1394,36,26), -- Final Passage, was 36/25
(1488,40,26), -- The Corrupter, was 40/25
(6627,30,26), -- Test of Lore, was 30/25
(6628,30,26), -- Test of Lore, was 30/25
(7788,25,20), -- Vanquish the Invaders!, was 29/20
(7789,25,20), -- Quell the Silverwing Usurpers, was 29/20
(7871,35,30), -- Vanquish the Invaders!, was 39/30
(7872,45,40), -- Vanquish the Invaders!, was 49/40
(7873,55,50), -- Vanquish the Invaders!, was 59/50
(7874,35,30), -- Quell the Silverwing Usurpers, was 39/30
(7875,45,40), -- Quell the Silverwing Usurpers, was 49/40
(7876,55,50), -- Quell the Silverwing Usurpers, was 59/50
(8080,55,50), -- Arathi Basin Resources!, was 59/50
(8123,55,50), -- Cut Arathor Supply Lines, was 59/50
(8154,45,40), -- Arathi Basin Resources!, was 49/40
(8155,25,20), -- Arathi Basin Resources!, was 29/20
(8156,35,30), -- Arathi Basin Resources!, was 39/30
(8160,45,40), -- Cut Arathor Supply Lines, was 49/40
(8161,35,30), -- Cut Arathor Supply Lines, was 39/30
(8162,25,20); -- Cut Arathor Supply Lines, was 29/20
UPDATE `quest_classic_level` q JOIN `tmp_quest_level` t ON t.`ID`=q.`ID` SET q.`QuestLevel`=t.`QuestLevel`, q.`MinLevel`=t.`MinLevel`;
DROP TEMPORARY TABLE `tmp_quest_level`;
