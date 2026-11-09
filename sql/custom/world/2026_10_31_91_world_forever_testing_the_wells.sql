-- Testing the Wells 92742 (Forever, Alliance, Westfall level 12): the quest is in the client but had no giver and no turn-in.
-- Turn-in: Alba Fairmoon 253092, from the official beta sniff merged by the Azeroth project
--   (Sakhwoow/WoWForever, GPL-2, sql/custom/world/2026_10_21_04_world_forever_sniff_paladin_03_quests.sql, build 70450).
-- Giver: the same NPC; our own saved Wowhead Forever quest page lists Alba Fairmoon 253092 as both start and end
--   (the sniff had no starter row because nobody was seen picking it up). Same NPC also takes 92744 in that sniff.
-- Needs Alba Fairmoon's template and spawn in the world DB (she is in 2026_10_30_10, guid 46000108, at 52.4,53).
-- Safe to run twice.
INSERT IGNORE INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (253092,92742,0);
INSERT IGNORE INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (253092,92742,70450),(253092,92744,70450);
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry`=253092;
