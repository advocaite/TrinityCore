-- Two Forever quest NPCs whose spawns could not be reached (Discord bug reports, 2026-11).
-- Heights measured with `.playerbot groundz` on the test realm (step 76, 2026-11-12).

-- Incinerator Gar'im (214519), Redridge, quest 95999 "Wanted: Incinerator Gar'im". His one spawn stood at 77.4, 85.8 on top of
-- the cliff wall (terrain there is 157.63), where nobody can reach him. Questie's Forever traces saw him 126 times, all in
-- Render's Valley (61-74, 68-86); the middle of those sightings is 69.4, 79.2, where the valley floor is 58.61. The sightings
-- cover a wide area, so he wanders further than the 5 yards he had (10 is INFERRED: the real path is unknown).
UPDATE `creature` SET `position_x` = -9721.46, `position_y` = -3188.88, `position_z` = 58.61, `orientation` = 0,
    `wander_distance` = 10, `MovementType` = 1
WHERE `guid` = 21451900 AND `id` = 214519;

-- Sentinel Lynessa Duskblossom (275744), Teldrassil, quest 99053 "Escaping Ban'ethil". The quest text has her captured and taken
-- down into Ban'ethil Barrow Den ("I am thankful that you have fought your way down here"); our import put her in a pool of three
-- spots up on the surface, none of them in the den. Questie's Forever traces see her at one spot only: 44.66, 58.92. Under that
-- point the floors are 1346.21 (surface), 1267.01 and 1257.51; the den floor is 1257.5 (Gnarlpine Augur 20049175 stands 2.6 yards
-- away at 1257.45, a Battered Chest at 1257.5). One spawn there, the pool goes.
DELETE FROM `pool_members` WHERE `type` = 0 AND `spawnId` IN (27574400, 27574401, 27574402);
DELETE FROM `pool_template` WHERE `entry` = 1302288;
DELETE FROM `creature_addon` WHERE `guid` IN (27574401, 27574402);
DELETE FROM `creature` WHERE `guid` IN (27574401, 27574402) AND `id` = 275744;
UPDATE `creature` SET `position_x` = 9831.65, `position_y` = 1540.64, `position_z` = 1257.51, `orientation` = 0,
    `wander_distance` = 0, `MovementType` = 0
WHERE `guid` = 27574400 AND `id` = 275744;
-- She is a prisoner among the furbolgs, so the furbolgs beside her must not fight her: immune to NPCs (512). INFERRED from the
-- quest text; without it she and the Augur next to her fight from the moment they spawn.
UPDATE `creature_template` SET `unit_flags` = `unit_flags` | 512 WHERE `entry` = 275744;
