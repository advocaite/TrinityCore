-- Camp Tent objects (gameobject 528996) at the adventurer camps: Goldshire, Brill, Kharanos, Razor Hill, Bloodhoof, Dolanaar and
-- Zephras Isle (Discord, Neylem 2026-11-07: floating tents in the starting zones). Like the Camp Tent creatures removed in
-- 2026_11_05_75, they are other players' camps a dump stored as fixed spawns; they came back with the quest data restore
-- 2026_11_05_99 (AnoCore spawns 52899600-52899606). The sniff has only the template, no spawn. Safe to run again.
DELETE FROM `gameobject_addon` WHERE `guid` IN (SELECT `guid` FROM `gameobject` WHERE `id` = 528996);
DELETE FROM `gameobject` WHERE `id` = 528996;
