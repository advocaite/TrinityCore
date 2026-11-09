-- Classic 1.60: 13 vanilla graveyards still had their retail (Cataclysm) position, so a released ghost appeared hundreds of
-- yards from the vanilla spirit healer (Darkshore 799 y, Azshara 898 / 1249 y, Feralas 518 y, Thousand Needles 495 y, ...),
-- in places such as New Auberdine that do not exist on the 1.60 map. The graveyard links (2026_09_30_01) are vanilla, only
-- the world_safe_locs rows were never converted.
-- Positions: WorldSafeLocs.dbc 3.3.5 (pre-Cataclysm, same old-world graveyards as 1.12), as dumped in AzerothCore's
-- game_graveyard; each one lands 4-21 y from the VMaNGOS spirit healer spawn. Facing: VMaNGOS world_safe_locs_facing
-- (radians, stored here in degrees); 789 has none there and keeps its own.
REPLACE INTO `world_safe_locs` (`ID`,`MapID`,`LocX`,`LocY`,`LocZ`,`Facing`,`Comment`) VALUES
(4,0,-10546.9,1197.24,31.7263,143.0,'Westfall, Sentinel Hill GY'),
(35,1,6739.19,209.993,23.2846,279.0,'Darkshore, New Auberdine GY'),
(39,1,-4656,-1765,-41,272.0,'Thousand Needles, The Great Lift GY (OLD)'),
(103,0,-6805,-2287.19,280.752,161.0,'Badlands, Kargath'),
(209,1,-7190.95,-3944.65,9.22739,155.0,'Tanaris, Gadgetzan GY'),
(309,1,-4596.4,3229.43,8.99376,72.0,'Feralas, New Feathermoon Stronghold GY (A)'),
(329,1,-5530.28,-3459.28,-45.7444,263.0,'Thousand Needles, Splithoof Heights GY (MOVED)'),
(369,1,2681.06,-4009.75,107.849,264.0,'Azshara, Northern Azshara GY'),
(409,1,898.261,434.53,65.7279,89.0,'Stonetalon Mountains, Webwinder Path GY'),
(450,1,-7205.56,-2436.67,-218.161,28.0,'Un''Goro Crater, The Marshlands'),
(469,1,4291.28,96.9557,43.0753,332.0,'Darkshore, Twilight Vale GY'),
(609,1,2942.76,-6037.13,5.16996,164.0,'Azshara, (Overlooks) The Shattered Strand GY'),
(789,0,-291,-4374,107,318.3,'The Hinterlands, The Overlook Cliffs');
