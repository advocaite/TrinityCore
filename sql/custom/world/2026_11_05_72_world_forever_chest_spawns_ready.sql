-- Chests that spawned already open. The Forever sniff caught 13 chest spawns in the "active" (open) state, 12 of the 17 Abandoned
-- Belongings crates of "Aid For The Refugees" (94896, Zephris Isle) and one Construct Parts: the client shows them but they cannot
-- be looted (Discord player report 2026-11-05). A chest (type 3) is always spawned ready (state 1), like every other chest in the
-- sniff (229 of 242). Safe to run again.
UPDATE `gameobject` g JOIN `gameobject_template` t ON t.`entry` = g.`id`
SET g.`state` = 1
WHERE t.`type` = 3 AND g.`state` = 0;
