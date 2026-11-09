-- Nimsy (248464, Fargodeep Mine): both spawns stood at the mine mouth, at the two outermost of Wowhead's (Forever) Nimsy points,
-- so she was met outside the cave. Moved to two of the points further in, on the upper floor (Elwynn map 39.5,80.3 and 39.7,79.3,
-- world position from the classic Elwynn Forest map bounds, which give exactly the old spawns for the old points).
-- Height from the kobold spawn next to each point (3.4 and 4.8 yd away); every spawn within 25 yd stands at 22-26, except one spider
-- on the hill above the mine. Check in game: .go creature id 248464 should put you inside the mine. Safe to run again.
UPDATE `creature` SET `position_x`=-9798.2, `position_y`=164.4, `position_z`=24.1 WHERE `guid`=24846400 AND `id`=248464;
UPDATE `creature` SET `position_x`=-9775.0, `position_y`=157.5, `position_z`=25.1 WHERE `guid`=24846401 AND `id`=248464;
