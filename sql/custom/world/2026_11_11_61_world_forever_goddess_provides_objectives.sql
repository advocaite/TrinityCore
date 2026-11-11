-- WoW Forever live realm (client 70888), capture dump_1.60.1.70888_2026-11-11_21-08-12.pkt (Russian client).
-- The Goddess Provides (97979, Shanda in Dolanaar, night elves only): on live it has two objectives, use the night elf racials
-- Light of Elune and Shadowmeld (criteria trees 231191 and 231193). Ours had no objective, so it could be turned in at once.
-- Objective ids, types, criteria trees and the Russian texts are live. The English texts are our own translation of the
-- Russian ones (no English capture yet). Kept in its own file: if the PC test shows the server can not credit the criteria
-- trees, leave this file out and the quest stays as it was. Safe to run more than once.
DELETE FROM `quest_objectives` WHERE `QuestID` = 97979;
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,
`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES
(476662,97979,14,0,0,231191,1,0,0,0,0,0,1,'Use Light of Elune',70888),
(476663,97979,14,1,1,231193,1,0,0,0,0,0,1,'Use your Shadowmeld ability',70888);
DELETE FROM `quest_objectives_locale` WHERE `ID` IN (476662,476663) AND `locale` = 'ruRU';
INSERT INTO `quest_objectives_locale` (`ID`,`locale`,`QuestId`,`StorageIndex`,`Description`,`VerifiedBuild`) VALUES
(476662,'ruRU',97979,0,'Примените "Свет Элуны"',70888),
(476663,'ruRU',97979,1,'Примените способность "Слиться с тенью"',70888);
