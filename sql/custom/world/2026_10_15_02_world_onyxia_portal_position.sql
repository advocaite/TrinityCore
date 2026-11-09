-- Onyxia's Lair portal: the real trigger 2848 (client AreaTrigger.db2, map 1, sphere radius 16) is centred at -4768.61, -3752.11, 53.4317.
-- The earlier spawn (2026_10_13_40) used a guessed position about 60 yd away, so players walked through the cave where no portal stood.
-- Sphere triggers sit 1.5 yd lower so the portal rests on the ground.
UPDATE `gameobject` SET `position_x`=-4768.61, `position_y`=-3752.11, `position_z`=51.9317 WHERE `guid`=21500028 AND `id`=1100001;
