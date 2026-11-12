-- Wetlands, Call of Water (shaman water chain): Hervdana Saegrund (258203) stood up in the rocks of the waterfall, out of reach, so
-- the quest could not be turned in and the next one not taken (Discord, Dmitrii 2026-11-11, screenshot at 65.4, 74.8). Her brother
-- Frothrik (258139) was placed the same way. Both spawns came with the restored Forever quest data (2026_11_05_99) at z 90.6 and 86.0,
-- but the ground there is at z 8-30 (the herb and fish spawns below the falls, and the family's Campfire 424066 at z 30.2, 11 yards
-- away), so they were set on the top of the rock instead of in the cave under it ("Her family dwells in a cave beneath the southern
-- falls", Call of Water 94495).
-- INFERRED: the spots. No sniff has them; both now stand 3 yards from the family's campfire, facing it, on its floor.
-- Safe to run again.
UPDATE `creature` SET `position_x` = -4262.81, `position_y` = -3117.90, `position_z` = 30.30, `orientation` = 3.1416 WHERE `guid` = 25820300 AND `id` = 258203;
UPDATE `creature` SET `position_x` = -4265.81, `position_y` = -3114.90, `position_z` = 30.30, `orientation` = 4.7124 WHERE `guid` = 25813900 AND `id` = 258139;
