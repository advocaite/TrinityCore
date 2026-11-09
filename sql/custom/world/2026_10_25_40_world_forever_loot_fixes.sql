-- Classic 1.60 (WoW Forever): loot rows that hand out a quest item twice, and loot ids that point at no loot.
--
-- 1. Duplicate quest item rows. The loot code rolls every row on its own, so two rows of one item give two chances and up to
--    two copies per kill or click. They come from the 70124 sniff imports:
--    - 2026_10_01_04 (sniff npc): the Pridewing creature query lists 277917 in two quest item slots, and a row was written
--      for each: Pridewing Wyvern 4012, Skyhunter 4013, Consort 4014 (40% twice, 64% in effect) and Patriarch 4015 (100% twice).
--    - 2026_10_01_03 (sniff): a quest item row for every gameobject that uses the loot id, plus the row from the sniffed
--      openings: Bottle 139709 (268801; objects 630871, 630885, "8 of 8 openings"), Smooth Boulder 142276 (277952, three rows:
--      objects 661446, 661448, 661449), Mirkweed and Dried Mirkweed Pile 142416 (278201), Gnarlpine Totem 142563 (279082).
--    Each keeps one row with the same chance (the sniffed row where there is one). The duplicate rows of vanilla loot (VMaNGOS
--    reference and group rows such as Gordok Tribute 16577 or the Silithid carapace bosses) are VMaNGOS data and stay.
-- 2. Ids with no loot table, left from the TDB base (VMaNGOS 1.12 has 0 for all of them):
--    - PickPocketLootID of Moonrage Watcher 1892, Moonrage Sentry 1893, Moonrage Elder 1896 (no pickpocketing_loot_template).
--    - SkinLootID 100008 of White Kitten 7386 and Pygmy Cockatrice 14869, and 11497 of The Razza 11497 (no
--      skinning_loot_template): the corpse showed as skinnable and gave nothing.
-- Safe to run again.

DELETE FROM `creature_loot_template` WHERE `Entry` IN (4012,4013,4014,4015) AND `ItemType` = 0 AND `Item` = 277917;
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(4012,0,277917,40,1,1,0,1,1,'quest item of the creature query (drop not seen in the sniffs)'),
(4013,0,277917,40,1,1,0,1,1,'quest item of the creature query (drop not seen in the sniffs)'),
(4014,0,277917,40,1,1,0,1,1,'quest item of the creature query (drop not seen in the sniffs)'),
(4015,0,277917,100,1,1,0,1,1,'quest item of the creature query (drop not seen in the sniffs)');

DELETE FROM `gameobject_loot_template` WHERE `ItemType` = 0 AND (`Entry`,`Item`) IN ((139709,268801),(142276,277952),(142416,278201),(142563,279082));
INSERT INTO `gameobject_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(139709,0,268801,100,1,1,0,1,1,'sniff: 8 of 8 openings (chests 630871 630885)'),
(142276,0,277952,100,1,1,0,1,1,'sniff: 8 of 8 openings (chests 661448 661449)'),
(142416,0,278201,100,1,1,0,1,1,'Mirkweed (sniff, quest item)'),
(142563,0,279082,100,1,1,0,1,1,'Gnarlpine Totem (sniff, quest item)');

UPDATE `creature_template_difficulty` SET `PickPocketLootID` = 0 WHERE `Entry` IN (1892,1893,1896) AND `PickPocketLootID` IN (1892,1893,1896)
    AND `PickPocketLootID` NOT IN (SELECT `Entry` FROM `pickpocketing_loot_template`);
UPDATE `creature_template_difficulty` SET `SkinLootID` = 0 WHERE `Entry` IN (7386,14869,11497) AND `SkinLootID` IN (100008,11497)
    AND `SkinLootID` NOT IN (SELECT `Entry` FROM `skinning_loot_template`);
