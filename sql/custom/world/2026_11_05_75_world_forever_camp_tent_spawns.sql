-- Camp Tent (263398) spawns are other players' camps that the Forever sniff (2026_10_03_00, 2026_09_27_00 baseline) saw and stored as
-- fixed spawns. They stood on the adventurer camps at Kharanos, Goldshire, Brill, Razor Hill and on Zephris Isle (two at one spot),
-- right on top of the quest NPCs at the campfire (Discord, 2026-11-05). Their campfire creature 242267 was already removed in
-- 2026_10_25_30. Safe to run again.
DELETE FROM `creature_addon` WHERE `guid` IN (SELECT `guid` FROM `creature` WHERE `id` = 263398);
DELETE FROM `creature` WHERE `id` = 263398;
