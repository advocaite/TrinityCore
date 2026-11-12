-- Zephras Isle, "Mercy Falls on Deaf Ears" (93165, Vayn Moongaze): the first ear turn-in asks for 5 Al'Aketh Cultist's Ears on
-- the live realm (client 1.60.1.70862 quest cache), but our row came from an early beta sniff (70124) and asked for 10, so
-- players saw 10 on the first turn-in and 5 on the repeatable "More Al'Aketh Ears" (93459) after it (player report 2026-11-01).
-- Data from Efficient Games (https://efficient.games), licensed under CC BY 4.0.
-- Zaal Stormshield (257196) is one of the 13 creatures players got the ears from on the live realm (Questie Forever item
-- traces, Questie/QuestieDB, GPLv3) and the only one without an ear row here. Chance INFERRED: the 30 we give the other
-- cultists that the sniffs did not cover.
-- Safe to run again.
UPDATE `quest_objectives` SET `Amount`=5 WHERE `ID`=466623 AND `QuestID`=93165;

DELETE FROM `creature_loot_template` WHERE `Entry`=257196 AND `ItemType`=0 AND `Item`=258771;
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(257196,0,258771,30,1,1,0,1,1,'Zaal Stormshield - Al''Aketh Cultist''s Ear (Questie Forever traces; chance INFERRED)');
