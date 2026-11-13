-- WoW Forever live realm: twelve Russian captures of 2026-11-12 (dump_1.60.1.70888_2026-11-12_09-13-39 to _21-38-32, Teldrassil,
-- Dolanaar and Darnassus) and dump_1.60.1.70938_2026-11-13_07-45-40.pkt (Darkshore, Auberdine). Only what our data lacks.
-- Comes after 2026_11_13_60. Chances are INFERRED from small samples: 3 for an item seen once. Safe to run more than once.

-- 1. Skinning was read as a drop. The earlier drop files counted a skinning window (loot type 6) as corpse loot when the corpse
--    window came first with nothing in it, so Light Leather (2318) and Ruined Leather Scraps (2934) were added as body drops on
--    skinnable beasts (100% on Juvenile Vuldren). Counting corpse windows only, none of these drops was ever seen. Their skinning
--    loot already has both items. Only our own rows are removed.
DELETE FROM `creature_loot_template` WHERE `ItemType` = 0 AND (`Entry`,`Item`) IN ((250868,2318),(250868,2934))
  AND `Comment` LIKE 'Forever live drop 2026-11-12 (%';
DELETE FROM `creature_loot_template` WHERE `ItemType` = 0 AND (`Entry`,`Item`) IN ((1138,2934),(250873,2934),(253282,2318),(256092,2318),(256092,2934),(258443,2318))
  AND `Comment` LIKE 'Forever live drop 2026-11-13 (%';
DELETE FROM `creature_loot_template` WHERE `ItemType` = 0 AND `Entry` = 1984 AND `Item` = 2934 AND `Comment` = 'live 70888 capture, chance inferred';

-- 2. Drops live gave that our mobs never drop (corpse windows only).
DELETE FROM `creature_loot_template` WHERE `Comment` LIKE 'Forever live drop 2026-11-13b (%';
INSERT IGNORE INTO `creature_loot_template` (`Entry`, `ItemType`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(2176, 0, 1413, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-13b (Cursed Highborne, 1 of 3 kills)'),
(2176, 0, 1418, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-13b (Cursed Highborne, 1 of 3 kills)'),
(2189, 0, 1422, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-13b (Vile Sprite, 1 of 18 kills)'),
(270196, 0, 17056, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-13b (Jai''vhanel, 1 of 1 kills)'),
(272034, 0, 1422, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-13b (Hatescreech, 1 of 2 kills)'),
(272034, 0, 3013, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-13b (Hatescreech, 1 of 2 kills)'),
(272044, 0, 118, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-13b (Windmistress Gaedress, 1 of 1 kills)'),
(272044, 0, 2213, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-13b (Windmistress Gaedress, 1 of 1 kills)'),
(272046, 0, 3287, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-13b (Witchmother Arysa, 1 of 1 kills)'),
(272046, 0, 17056, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-13b (Witchmother Arysa, 1 of 1 kills)'),
(272096, 0, 4669, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-13b (Befouled Webwood, 1 of 8 kills)'),
(272096, 0, 4686, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-13b (Befouled Webwood, 1 of 8 kills)'),
(275703, 0, 159, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-13b (Lasher Sproutling, 1 of 8 kills)'),
(275703, 0, 765, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-13b (Lasher Sproutling, 1 of 8 kills)'),
(275703, 0, 1411, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-13b (Lasher Sproutling, 1 of 8 kills)'),
(275703, 0, 2449, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-13b (Lasher Sproutling, 1 of 8 kills)');

-- 3. Vendor items live sells that ours lacks (rows we have and live did not show stay: they may be seasonal or conditional).
--    Laird in Auberdine sells five raw fish in stacks of 5 that restock; the restock time (15 min) is INFERRED.
DELETE FROM `npc_vendor` WHERE (`entry`,`item`) IN ((3614,4536),(4170,11291),(4200,6289),(4200,6308),(4200,6362),(4200,4603),(4200,8959),(4265,10284),(4265,4471),(4265,4470),(4265,11291),(4265,4536));
INSERT INTO `npc_vendor` (`entry`, `slot`, `item`, `maxcount`, `incrtime`, `ExtendedCost`, `type`, `BonusListIDs`, `PlayerConditionID`, `IgnoreFiltering`, `VerifiedBuild`) VALUES
(3614, 15, 4536, 0, 0, 0, 1, NULL, 0, 0, 70888),
(4170, 19, 11291, 0, 0, 0, 1, NULL, 0, 0, 70888),
(4200, 15, 6289, 5, 900, 0, 1, NULL, 0, 0, 70888),
(4200, 16, 6308, 5, 900, 0, 1, NULL, 0, 0, 70888),
(4200, 17, 6362, 5, 900, 0, 1, NULL, 0, 0, 70888),
(4200, 18, 4603, 5, 900, 0, 1, NULL, 0, 0, 70888),
(4200, 19, 8959, 5, 900, 0, 1, NULL, 0, 0, 70888),
(4265, 5, 10284, 0, 0, 0, 1, NULL, 0, 0, 70888),
(4265, 6, 4471, 0, 0, 0, 1, NULL, 0, 0, 70888),
(4265, 7, 4470, 0, 0, 0, 1, NULL, 0, 0, 70888),
(4265, 8, 11291, 0, 0, 0, 1, NULL, 0, 0, 70888),
(4265, 12, 4536, 0, 0, 0, 1, NULL, 0, 0, 70888);

-- 4. Quest givers live uses that ours lacks: Moon Priestess Amara (2151) also gives Seek Redemption! (489), and the Oracle Tree
--    Bark (673378) gives Teldrassil (940) right after The Oracle Tree (98398) is handed in there.
DELETE FROM `creature_queststarter` WHERE `id` = 2151 AND `quest` = 489;
INSERT INTO `creature_queststarter` (`id`, `quest`, `VerifiedBuild`) VALUES (2151, 489, 70888);
DELETE FROM `gameobject_queststarter` WHERE `id` = 673378 AND `quest` = 940;
INSERT INTO `gameobject_queststarter` (`id`, `quest`, `VerifiedBuild`) VALUES (673378, 940, 70888);
