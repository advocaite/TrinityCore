-- Classic 1.60 (WoW Forever): Westfall spawns sunk into the floor (bug-report forum 2026-11-13, voldemargreed, "bugs 3" items 5 and 12).
-- These spawns came back with the Forever quest data (2026_11_05_99) with heights below the floor. New heights are the navmesh
-- floor at the same spot (mmap_probe on the test realm, 2026-11-13; normal search from the old height, not the wide search):
--   Alba Fairmoon (253092), Sentinel Hill inn          33.06  -> 35.28  (shown chest-deep in the floorboards)
--   Suspicious Crate (581777), 42,68 near Moonbrook    43.443 -> 45.03
--   Suspicious Crate (581777), Moonbrook                33.618 -> 34.65
-- Safe to run again.
UPDATE `creature` SET `position_z` = 35.28 WHERE `guid` = 25309200 AND `id` = 253092;
UPDATE `gameobject` SET `position_z` = 45.03 WHERE `guid` = 58177704 AND `id` = 581777;
UPDATE `gameobject` SET `position_z` = 34.65 WHERE `guid` = 58177724 AND `id` = 581777;
