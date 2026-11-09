-- Classic 1.60: the two VMaNGOS patrol paths of the Pridewing Soarers in Stonetalon Mountains (VMaNGOS spawns 32374 and 32376,
-- creature_movement of the VMaNGOS 2026 world DB: two points each, 30 s pause at each). They were imported as standing spawns because
-- the path was not in the data used then. Path id = 800,000,000 + VMaNGOS guid * 10, as for the other VMaNGOS spawn paths. The
-- other 70 spawns that VMaNGOS marks as patrolling follow a patrolling formation leader (creature_formations) and have no path of
-- their own, or have no path in VMaNGOS either. Safe to run again.
DELETE FROM `waypoint_path_node` WHERE `PathId` IN (800323740, 800323760);
DELETE FROM `waypoint_path` WHERE `PathId` IN (800323740, 800323760);
INSERT INTO `waypoint_path` (`PathId`,`MoveType`,`Flags`,`Velocity`,`Comment`) VALUES
(800323740,0,0,NULL,'VMaNGOS 1.12 spawn path'),
(800323760,0,0,NULL,'VMaNGOS 1.12 spawn path');
INSERT INTO `waypoint_path_node` (`PathId`,`NodeId`,`PositionX`,`PositionY`,`PositionZ`,`Orientation`,`Delay`) VALUES
(800323740,1,1274.96,998.782,261.414,NULL,30000),
(800323740,2,1395.66,1133.55,253.876,NULL,30000),
(800323760,1,1588.57,872.573,184.32,NULL,30000),
(800323760,2,1767.44,1039.39,228.905,NULL,30000);
DELETE FROM `creature_addon` WHERE `guid` IN (20032374, 20032376);
INSERT INTO `creature_addon` (`guid`,`PathId`) VALUES (20032374,800323740),(20032376,800323760);
UPDATE `creature` SET `MovementType` = 2, `wander_distance` = 0 WHERE `guid` IN (20032374, 20032376);
