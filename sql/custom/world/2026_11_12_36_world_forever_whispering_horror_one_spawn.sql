-- Whispering Horror (260396, Forever elite at the Shadowvale crypt, Tirisfal; drops Whispering Horror Residue for quest 95328):
-- one at a time instead of nine (duplicates found by SIN).
-- Two imports each placed it on the same map pins: 46000004-46000007 (quest giver spawns, already moved down to the crypt floor
-- by 2026_11_03_40, where the sniffed camp mobs stand at z 108) and 26039600-26039604 (the AnoCore port in 2026_11_05_99, the
-- same spots but on the hill 50-100 yd above the crypt). So nine Horrors were up at once, five of them above the camp.
-- The hill copies go. The four crypt spots stay as one pool with one Horror up at a time, so it still turns up at any of the
-- pinned spots. INFERRED: whether Forever has one Horror or one per pin is not in any sniff we have; the pins come from Questie.
-- Safe to run again.
DELETE FROM `spawn_group` WHERE `spawnType` = 0 AND `spawnId` IN (26039600,26039601,26039602,26039603,26039604,46000004,46000005,46000006,46000007);
DELETE FROM `creature_addon` WHERE `guid` IN (26039600,26039601,26039602,26039603,26039604);
DELETE FROM `creature` WHERE `guid` IN (26039600,26039601,26039602,26039603,26039604) AND `id` = 260396;

DELETE FROM `pool_members` WHERE `type` = 0 AND `spawnId` IN (46000004,46000005,46000006,46000007);
DELETE FROM `pool_template` WHERE `entry` = 2603960;
INSERT INTO `pool_template` (`entry`,`max_limit`,`description`) VALUES (2603960,1,'Tirisfal - Whispering Horror, one of four crypt spots');
INSERT INTO `pool_members` (`type`,`spawnId`,`poolSpawnId`,`chance`,`description`) VALUES
(0,46000004,2603960,0,'Whispering Horror'),
(0,46000005,2603960,0,'Whispering Horror'),
(0,46000006,2603960,0,'Whispering Horror'),
(0,46000007,2603960,0,'Whispering Horror');
