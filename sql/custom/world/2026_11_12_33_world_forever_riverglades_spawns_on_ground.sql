-- Riverglades: 57 creatures stood off the walkable ground, so they never moved (Discord #общий, _yoshiro_: "mobs just stand still").
-- Their heights were guessed from our nearest spawns (2026_11_08_96, 2026_11_10_73), and many were 6 to 70 yd above or below the
-- ground. Random movement needs a navmesh path from where the creature stands, and none is found from more than 5 yd off the mesh.
-- New spots come from the server's own map 0 navmesh (mmap_probe, PC steps 90 and 92): the walkable point straight above or below
-- the old spot, nearest to the old height. 4 spots had none straight up or down and use the nearest walkable point (4 to 12 yd away).
-- INFERRED: 47200062-47200080 (shore, 255898/255899/255901) end up at sea level (0 to 6), likely the water surface: they will swim.
-- Each row checks guid and entry, so it only moves our own spawns. Safe to run again.

UPDATE `creature` SET `position_x`=-8077.920, `position_y`=-4208.930, `position_z`=26.627 WHERE `guid`=47200000 AND `id`=254560; -- was z 27.7
UPDATE `creature` SET `position_x`=-8120.590, `position_y`=-4181.000, `position_z`=29.678 WHERE `guid`=47200003 AND `id`=254560; -- was z 20.45
UPDATE `creature` SET `position_x`=-8174.250, `position_y`=-4084.620, `position_z`=23.410 WHERE `guid`=47200005 AND `id`=254560; -- was z 9.87
UPDATE `creature` SET `position_x`=-8642.660, `position_y`=-3385.720, `position_z`=162.779 WHERE `guid`=47200007 AND `id`=255770; -- was z 172.3
UPDATE `creature` SET `position_x`=-8598.690, `position_y`=-3344.640, `position_z`=163.042 WHERE `guid`=47200008 AND `id`=255770; -- was z 172.43
UPDATE `creature` SET `position_x`=-8291.560, `position_y`=-3379.260, `position_z`=163.755 WHERE `guid`=47200012 AND `id`=255770; -- was z 171.35
UPDATE `creature` SET `position_x`=-8615.830, `position_y`=-3411.800, `position_z`=159.840 WHERE `guid`=47200015 AND `id`=255770; -- was z 172.17
UPDATE `creature` SET `position_x`=-9074.000, `position_y`=-4364.510, `position_z`=10.658 WHERE `guid`=47200020 AND `id`=258573; -- was z 19.89
UPDATE `creature` SET `position_x`=-9001.910, `position_y`=-4343.190, `position_z`=13.495 WHERE `guid`=47200021 AND `id`=258573; -- was z 21.34
UPDATE `creature` SET `position_x`=-8970.550, `position_y`=-4322.110, `position_z`=10.803 WHERE `guid`=47200022 AND `id`=258573; -- was z 21.87
UPDATE `creature` SET `position_x`=-8933.040, `position_y`=-4345.700, `position_z`=12.233 WHERE `guid`=47200023 AND `id`=258573; -- was z 22.27
UPDATE `creature` SET `position_x`=-8925.610, `position_y`=-4326.210, `position_z`=10.536 WHERE `guid`=47200024 AND `id`=258573; -- was z 22.28
UPDATE `creature` SET `position_x`=-8992.850, `position_y`=-4314.470, `position_z`=10.564 WHERE `guid`=47200025 AND `id`=258573; -- was z 21.73
UPDATE `creature` SET `position_x`=-8967.950, `position_y`=-4248.720, `position_z`=11.141 WHERE `guid`=47200026 AND `id`=258573; -- was z 25.61
UPDATE `creature` SET `position_x`=-8992.520, `position_y`=-4244.490, `position_z`=11.343 WHERE `guid`=47200027 AND `id`=258573; -- was z 25.59
UPDATE `creature` SET `position_x`=-9004.810, `position_y`=-4284.410, `position_z`=10.705 WHERE `guid`=47200028 AND `id`=258573; -- was z 21.97
UPDATE `creature` SET `position_x`=-9039.080, `position_y`=-4267.120, `position_z`=10.557 WHERE `guid`=47200029 AND `id`=258573; -- was z 22.73
UPDATE `creature` SET `position_x`=-9043.290, `position_y`=-4294.840, `position_z`=11.234 WHERE `guid`=47200031 AND `id`=258573; -- was z 22.03
UPDATE `creature` SET `position_x`=-8994.790, `position_y`=-4330.510, `position_z`=13.077 WHERE `guid`=47200032 AND `id`=258573; -- was z 21.56
UPDATE `creature` SET `position_x`=-8890.370, `position_y`=-4276.930, `position_z`=10.759 WHERE `guid`=47200033 AND `id`=258573; -- was z 22.82
UPDATE `creature` SET `position_x`=-9010.630, `position_y`=-4273.750, `position_z`=10.701 WHERE `guid`=47200042 AND `id`=258573; -- was z 22.09
UPDATE `creature` SET `position_x`=-8844.780, `position_y`=-4307.270, `position_z`=11.557 WHERE `guid`=47200043 AND `id`=258573; -- was z 26.32
UPDATE `creature` SET `position_x`=-8916.190, `position_y`=-3655.580, `position_z`=73.053 WHERE `guid`=47200044 AND `id`=259051; -- was z 51.43
UPDATE `creature` SET `position_x`=-8120.920, `position_y`=-4179.550, `position_z`=29.713 WHERE `guid`=47200045 AND `id`=259158; -- was z 20.44
UPDATE `creature` SET `position_x`=-8791.050, `position_y`=-3323.430, `position_z`=158.883 WHERE `guid`=47200047 AND `id`=262473; -- was z 110.46
UPDATE `creature` SET `position_x`=-8802.218, `position_y`=-3296.070, `position_z`=163.324 WHERE `guid`=47200048 AND `id`=262473; -- was z 110.46
UPDATE `creature` SET `position_x`=-8786.250, `position_y`=-3293.440, `position_z`=165.467 WHERE `guid`=47200049 AND `id`=262473; -- was z 110.46
UPDATE `creature` SET `position_x`=-8824.030, `position_y`=-3307.100, `position_z`=166.147 WHERE `guid`=47200050 AND `id`=262473; -- was z 110.46
UPDATE `creature` SET `position_x`=-8849.250, `position_y`=-3373.830, `position_z`=180.067 WHERE `guid`=47200051 AND `id`=262473; -- was z 110.46
UPDATE `creature` SET `position_x`=-8794.266, `position_y`=-3335.279, `position_z`=159.683 WHERE `guid`=47200052 AND `id`=262473; -- was z 110.46, moved 8 yd
UPDATE `creature` SET `position_x`=-8821.450, `position_y`=-3372.690, `position_z`=180.836 WHERE `guid`=47200053 AND `id`=262477; -- was z 110.46
UPDATE `creature` SET `position_x`=-8619.710, `position_y`=-3404.540, `position_z`=159.771 WHERE `guid`=47200054 AND `id`=255772; -- was z 172.2
UPDATE `creature` SET `position_x`=-8456.570, `position_y`=-5338.540, `position_z`=6.355 WHERE `guid`=47200062 AND `id`=255898; -- was z 16.39
UPDATE `creature` SET `position_x`=-8962.190, `position_y`=-5176.320, `position_z`=0.750 WHERE `guid`=47200063 AND `id`=255898; -- was z 14.83
UPDATE `creature` SET `position_x`=-8967.690, `position_y`=-5177.330, `position_z`=1.205 WHERE `guid`=47200070 AND `id`=255899; -- was z 14.9
UPDATE `creature` SET `position_x`=-8784.710, `position_y`=-5256.900, `position_z`=1.456 WHERE `guid`=47200072 AND `id`=255899; -- was z 7.74
UPDATE `creature` SET `position_x`=-8945.060, `position_y`=-5132.000, `position_z`=0.011 WHERE `guid`=47200077 AND `id`=255901; -- was z 21.19
UPDATE `creature` SET `position_x`=-8950.220, `position_y`=-5020.270, `position_z`=0.305 WHERE `guid`=47200080 AND `id`=255901; -- was z 28.57
UPDATE `creature` SET `position_x`=-8993.820, `position_y`=-4344.110, `position_z`=13.566 WHERE `guid`=47200100 AND `id`=258574; -- was z 21.46
UPDATE `creature` SET `position_x`=-8976.690, `position_y`=-4325.550, `position_z`=10.882 WHERE `guid`=47200101 AND `id`=258574; -- was z 21.79
UPDATE `creature` SET `position_x`=-8895.220, `position_y`=-4338.180, `position_z`=10.536 WHERE `guid`=47200102 AND `id`=258574; -- was z 26.58
UPDATE `creature` SET `position_x`=-8933.690, `position_y`=-4356.390, `position_z`=13.150 WHERE `guid`=47200103 AND `id`=258574; -- was z 22.33
UPDATE `creature` SET `position_x`=-8926.570, `position_y`=-4249.440, `position_z`=12.308 WHERE `guid`=47200104 AND `id`=258574; -- was z 25.62
UPDATE `creature` SET `position_x`=-8395.380, `position_y`=-3982.430, `position_z`=10.470 WHERE `guid`=47200106 AND `id`=258574; -- was z 18.92
UPDATE `creature` SET `position_x`=-8719.330, `position_y`=-4129.160, `position_z`=10.668 WHERE `guid`=47200108 AND `id`=258588; -- was z 18.34
UPDATE `creature` SET `position_x`=-8204.330, `position_y`=-4275.290, `position_z`=10.470 WHERE `guid`=47200111 AND `id`=258588; -- was z 18.6
UPDATE `creature` SET `position_x`=-9174.880, `position_y`=-4560.940, `position_z`=16.252 WHERE `guid`=47200114 AND `id`=259267; -- was z 7.84
UPDATE `creature` SET `position_x`=-9200.420, `position_y`=-4539.220, `position_z`=16.261 WHERE `guid`=47200119 AND `id`=259267; -- was z 9.8
UPDATE `creature` SET `position_x`=-9271.230, `position_y`=-4563.460, `position_z`=11.729 WHERE `guid`=47200120 AND `id`=259267; -- was z 33.35
UPDATE `creature` SET `position_x`=-7333.237, `position_y`=-5691.319, `position_z`=69.168 WHERE `guid`=47221002 AND `id`=257910; -- was z 64.16, moved 12 yd
UPDATE `creature` SET `position_x`=-7334.990, `position_y`=-5680.430, `position_z`=76.971 WHERE `guid`=47221003 AND `id`=257901; -- was z 64.16
UPDATE `creature` SET `position_x`=-7348.090, `position_y`=-5665.300, `position_z`=79.244 WHERE `guid`=47221004 AND `id`=257910; -- was z 64.16
UPDATE `creature` SET `position_x`=-7362.940, `position_y`=-5671.600, `position_z`=81.626 WHERE `guid`=47221005 AND `id`=257901; -- was z 64.16
UPDATE `creature` SET `position_x`=-7368.860, `position_y`=-5724.540, `position_z`=56.918 WHERE `guid`=47221012 AND `id`=257910; -- was z 64.16
UPDATE `creature` SET `position_x`=-7358.870, `position_y`=-5729.530, `position_z`=56.586 WHERE `guid`=47221013 AND `id`=257901; -- was z 64.16
UPDATE `creature` SET `position_x`=-7344.267, `position_y`=-5735.200, `position_z`=57.605 WHERE `guid`=47221014 AND `id`=257910; -- was z 64.16, moved 4 yd
UPDATE `creature` SET `position_x`=-7338.667, `position_y`=-5717.066, `position_z`=63.738 WHERE `guid`=47221016 AND `id`=257910; -- was z 64.16, moved 9 yd
