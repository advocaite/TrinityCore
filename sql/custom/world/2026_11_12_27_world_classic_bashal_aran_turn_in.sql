-- Bashal'Aran (954) could not be finished (player report 2026-11-13, Discord "bugs 2" item 4). It is a "go there" quest: the
-- player walks to Asterion at Bashal'Aran (client area trigger 230, 13 yards, still in the 1.60 AreaTrigger data). The quest only
-- had the completion event flag (0x2) and no trigger row, and the server keeps an areatrigger_involvedrelation row only for a
-- quest with the area trigger completion flag (0x4), so nothing ever completed it. Same fix as The Defias Brotherhood (155) in
-- 2026_11_02_00.
-- Safe to run again.

UPDATE `quest_template` SET `Flags` = `Flags` | 0x4 WHERE `ID` = 954;

DELETE FROM `areatrigger_involvedrelation` WHERE `id` = 230 AND `quest` = 954;
INSERT INTO `areatrigger_involvedrelation` (`id`,`quest`) VALUES (230,954);
