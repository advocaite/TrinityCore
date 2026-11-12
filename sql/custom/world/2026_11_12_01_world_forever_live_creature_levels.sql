-- Creature levels from the live realm for NPCs whose entry Forever gave a new role (Discord, Russian report 2026-11-10: "extra NPC
-- 14242 by the Golden Path on the way to the lifts, gives quest 98093").
-- Sulhasa 14242 is a level 20 Horde quest giver in the southern Barrens on Forever (Field to Clear 98093, "up in a tree" on the
-- Camp Taurajo road). Our data still gave her the vanilla level 55 and her vanilla melee hits (298-427), from the 2026_10_11_00 /
-- 2026_10_11_01 tables, so a level 55 quest giver stood by the road. Four more quest givers had the same fault; these five are all
-- the NPCs where the live level (EG Link npcs-wowf 70334-70888, data from Efficient Games, CC BY 4.0) lies outside ours by more
-- than 2 levels (bosses shown as ?? on live left out):
--   14242 Sulhasa 55 -> 20, 13155 Deathstalker Agent (Wetlands) 61 -> 40, 3682 Vrang Wildgore 48 -> 35,
--   823 Deputy Willem (Northshire) 18 -> 1, 267214 Va'xug Firefure 22 -> 28.
-- The vanilla melee row of the four vanilla entries is dropped, so their hits follow the new level (class level stats times their
-- multipliers) instead of the vanilla creature's. Safe to run again.
UPDATE `creature_classic_level` SET `level_min` = 20, `level_max` = 20 WHERE `entry` = 14242;
UPDATE `creature_classic_level` SET `level_min` = 40, `level_max` = 40 WHERE `entry` = 13155;
UPDATE `creature_classic_level` SET `level_min` = 35, `level_max` = 35 WHERE `entry` = 3682;
UPDATE `creature_classic_level` SET `level_min` = 1, `level_max` = 1 WHERE `entry` = 823;
UPDATE `creature_classic_level` SET `level_min` = 28, `level_max` = 28 WHERE `entry` = 267214;
DELETE FROM `creature_classic_melee` WHERE `entry` IN (14242, 13155, 3682, 823);
