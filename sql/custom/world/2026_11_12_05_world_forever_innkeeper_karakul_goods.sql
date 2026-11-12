-- Innkeeper Karakul (6930): the live gossip menu offers "I want to browse your goods", but he had no vendor flag and no goods, so
-- the option opened nothing (PC data test 49, 2026-11-12: NPCs with a live vendor option and no npc_vendor rows).
-- Goods: the classic innkeeper list for him from the AzerothCore world database (npc_vendor 6930), all vanilla items that exist in
-- this client. Safe to run again.
UPDATE `creature_template` SET `npcflag` = `npcflag` | 128 WHERE `entry` = 6930;
DELETE FROM `npc_vendor` WHERE `entry` = 6930;
INSERT INTO `npc_vendor` (`entry`, `slot`, `item`, `maxcount`, `incrtime`, `ExtendedCost`, `type`, `BonusListIDs`, `PlayerConditionID`, `IgnoreFiltering`, `VerifiedBuild`) VALUES
(6930, 0, 159, 0, 0, 0, 1, NULL, 0, 0, 0),    -- Refreshing Spring Water
(6930, 0, 1179, 0, 0, 0, 1, NULL, 0, 0, 0),   -- Ice Cold Milk
(6930, 0, 1205, 0, 0, 0, 1, NULL, 0, 0, 0),   -- Melon Juice
(6930, 0, 1708, 0, 0, 0, 1, NULL, 0, 0, 0),   -- Sweet Nectar
(6930, 0, 1645, 0, 0, 0, 1, NULL, 0, 0, 0),   -- Moonberry Juice
(6930, 0, 8766, 0, 0, 0, 1, NULL, 0, 0, 0),   -- Morning Glory Dew
(6930, 0, 414, 0, 0, 0, 1, NULL, 0, 0, 0),    -- Dalaran Sharp
(6930, 0, 422, 0, 0, 0, 1, NULL, 0, 0, 0),    -- Dwarven Mild
(6930, 0, 1707, 0, 0, 0, 1, NULL, 0, 0, 0),   -- Stormwind Brie
(6930, 0, 2070, 0, 0, 0, 1, NULL, 0, 0, 0),   -- Darnassian Bleu
(6930, 0, 3927, 0, 0, 0, 1, NULL, 0, 0, 0),   -- Fine Aged Cheddar
(6930, 0, 8932, 0, 0, 0, 1, NULL, 0, 0, 0);   -- Alterac Swiss
