-- WoW Forever live realm (client 70888): English capture dump_1.60.1.70888_2026-11-11_19-21-09.pkt (336k packets), Zephras Isle
-- (Shen'dar Village, Falaath, Shen'dar Highlands). Only what our data lacks. Its 14 vendors, 35 quests, gossip menus and trainers
-- match ours (or differ only by the player's class or a conditional option); the drops below are new. Chances are INFERRED from
-- small samples: 3 for an item seen once, 10 for twice, otherwise the seen share rounded to 5. Safe to run more than once.

-- 1. Drops live gave that our Zephras mobs never drop (mostly vanilla greys and whites). Rows we already have stay.
DELETE FROM `creature_loot_template` WHERE `Comment` LIKE 'Forever live drop 2026-11-12 (%';
INSERT IGNORE INTO `creature_loot_template` (`Entry`, `ItemType`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(250868, 0, 1416, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Vuldren, 1 of 8 kills)'),
(250868, 0, 2318, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Vuldren, 1 of 8 kills)'),
(250868, 0, 2934, 40, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Vuldren, 3 of 8 kills)'),
(250873, 0, 1370, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Juvenile Vuldren, 1 of 11 kills)'),
(250873, 0, 1378, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Juvenile Vuldren, 1 of 11 kills)'),
(250873, 0, 2649, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Juvenile Vuldren, 1 of 11 kills)'),
(250937, 0, 1370, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Ursera Scavenger, 1 of 5 kills)'),
(251115, 0, 2650, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Urs''anah, 1 of 1 kills)'),
(251143, 0, 1366, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Roiling Winds, 1 of 9 kills)'),
(251143, 0, 1369, 10, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Roiling Winds, 2 of 9 kills)'),
(251143, 0, 2656, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Roiling Winds, 1 of 9 kills)'),
(251160, 0, 1368, 20, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Al''Aketh Convert, 3 of 14 kills)'),
(251160, 0, 1372, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Al''Aketh Convert, 1 of 14 kills)'),
(251160, 0, 2211, 10, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Al''Aketh Convert, 2 of 14 kills)'),
(251160, 0, 2653, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Al''Aketh Convert, 1 of 14 kills)'),
(251169, 0, 1377, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Pesky Cirrusfly, 1 of 8 kills)'),
(251245, 0, 1367, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Prideclaw, 1 of 26 kills)'),
(251245, 0, 2652, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Prideclaw, 1 of 26 kills)'),
(251245, 0, 3363, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Prideclaw, 1 of 26 kills)'),
(251284, 0, 1431, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Hippogryph Protector, 1 of 11 kills)'),
(251284, 0, 2773, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Hippogryph Protector, 1 of 11 kills)'),
(251291, 0, 1416, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Hippogryph Youth, 1 of 10 kills)'),
(251291, 0, 1418, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Hippogryph Youth, 1 of 10 kills)'),
(251402, 0, 1364, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Cirrusfly Soldier, 1 of 1 kills)'),
(251448, 0, 1368, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Al''Aketh Neophyte, 1 of 4 kills)'),
(251661, 0, 774, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Galestrider, 1 of 21 kills)'),
(251661, 0, 1376, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Galestrider, 1 of 21 kills)'),
(251661, 0, 2773, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Galestrider, 1 of 21 kills)'),
(251918, 0, 1417, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Highlands Bandit, 1 of 18 kills)'),
(251918, 0, 1429, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Highlands Bandit, 1 of 18 kills)'),
(251918, 0, 1431, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Highlands Bandit, 1 of 18 kills)'),
(254589, 0, 3300, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Vulgara the Insatiable, 1 of 2 kills)'),
(255534, 0, 252031, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 ("Badwind" Bennic, 1 of 1 kills)'),
(256935, 0, 1368, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Malduko Cloudcrush, 1 of 1 kills)'),
(257532, 0, 1431, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Windshaper Novice Seer, 1 of 7 kills)'),
(257532, 0, 2649, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Windshaper Novice Seer, 1 of 7 kills)'),
(271712, 0, 7096, 10, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Cloudrunner, 2 of 3 kills)');

-- 2. Windstone (616907) and Dented Chest (626752): live sends the same loot ids as ours (137592, 139194), but our loot tables were
--    empty, so both opened with nothing. Windstone gave item 255663 on all 3 openings (100%). The Dented Chest was opened once and
--    gave 6 items; each gets 35% (INFERRED) until more openings are seen.
DELETE FROM `gameobject_loot_template` WHERE `Comment` LIKE 'Forever live drop 2026-11-12 (%';
INSERT IGNORE INTO `gameobject_loot_template` (`Entry`, `ItemType`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(137592, 0, 255663, 100, 0, 1, 0, 1, 3, 'Forever live drop 2026-11-12 (Windstone, 3 of 3 openings)'),
(139194, 0, 1414, 35, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Dented Chest, 1 of 1 openings)'),
(139194, 0, 2835, 35, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Dented Chest, 1 of 1 openings)'),
(139194, 0, 4565, 35, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Dented Chest, 1 of 1 openings)'),
(139194, 0, 252031, 35, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Dented Chest, 1 of 1 openings)'),
(139194, 0, 252032, 35, 0, 1, 0, 2, 2, 'Forever live drop 2026-11-12 (Dented Chest, 1 of 1 openings)'),
(139194, 0, 267474, 35, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-12 (Dented Chest, 1 of 1 openings)');
