-- Classic 1.60: Blackfathom Deeps bosses were missing their Forever blues (Lorgus Jett's whole set 273841/273843/273842,
-- plus Kelris 273846, Ghamoo-ra 273839, Gelihast 273840); the single-use references are inlined together with the new
-- items so each boss drops exactly one of its own set per kill.
-- rollback: re-insert the reference pointers (4832,1,30153),(4887,1,30156),(6243,1,30178) each with
--   Chance=100,QuestRequired=0,GroupId=0,MinCount=1,MaxCount=1,Comment='VMaNGOS 1.12', then DELETE the rows inserted below.
DELETE FROM `creature_loot_template` WHERE `Entry` IN (4832,4887,6243) AND `ItemType` = 1 AND `Item` IN (30153,30156,30178);
DELETE FROM `creature_loot_template` WHERE `Entry` IN (4832,4887,6243,12902) AND `ItemType` = 0 AND `GroupId` = 1
  AND `Item` IN (1155,6903,273846,6907,6908,273839,6905,6906,273840,273841,273843,273842);
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(4832,0,1155,0,0,1,1,1,1,'Twilight Lord Kelris - Rod of the Sleepwalker'),
(4832,0,6903,0,0,1,1,1,1,'Twilight Lord Kelris - Gaze Dreamer Pants'),
(4832,0,273846,0,0,1,1,1,1,'Twilight Lord Kelris - Twilight Lord Girdle'),
(4887,0,6907,0,0,1,1,1,1,'Ghamoo-ra - Tortoise Armor'),
(4887,0,6908,0,0,1,1,1,1,'Ghamoo-ra - Ghamoo-ra\'s Bind'),
(4887,0,273839,0,0,1,1,1,1,'Ghamoo-ra - Spiked Shell Band'),
(6243,0,6905,0,0,1,1,1,1,'Gelihast - Reef Axe'),
(6243,0,6906,0,0,1,1,1,1,'Gelihast - Algae Fists'),
(6243,0,273840,0,0,1,1,1,1,'Gelihast - Cursed Murloc Eye'),
(12902,0,273841,0,0,1,1,1,1,'Lorgus Jett - Twilight Maul'),
(12902,0,273843,0,0,1,1,1,1,'Lorgus Jett - Fallenroot Longbow'),
(12902,0,273842,0,0,1,1,1,1,'Lorgus Jett - Treacherous Treads');