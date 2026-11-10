-- Lyreena Duskblade's campfire (Teldrassil adventurer camp, 2026_10_28_80): in game the camp's fire glow is 17 yards south-west
-- of the inferred spot, in front of the two tents, and the spawned Basic Campfire sat 0.7 yards under the ground there. The fire
-- moves onto the glow (GM .gps on it 2026-10-28: 9905.87, 877.39, ground 1313.91). Sitting by it still gives Boosted Rest
-- (spell focus 4, 15 yards) for The Great Outdoors 96606.
UPDATE `gameobject` SET `position_x`=9905.87, `position_y`=877.39, `position_z`=1313.91 WHERE `guid`=52916100 AND `id`=529161;
