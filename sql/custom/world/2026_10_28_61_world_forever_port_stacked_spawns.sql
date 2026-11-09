-- Forever NPC port (2026_10_26_35) spawns that stand on top of an existing spawn.
-- The port skipped only spawns next to a same-name spawn, so where the old data picks one of
-- several creatures for a spot, the port added a second creature at that same spot.

-- Dustwallow Marsh: Noxious Reavers/Shredders stacked on spots that already pick between
-- Darkfang creatures. They join the spot's pool, so one creature stands there.
DELETE FROM `pool_members` WHERE `type`=0 AND `spawnId` IN (434710,434711,434800,434804,434814,434815,434816,434823,20033764);
INSERT INTO `pool_members` (`type`,`spawnId`,`poolSpawnId`,`chance`,`description`) VALUES
(0,434710,1031692,0,'Forever port alternative'),
(0,434800,1030722,0,'Forever port alternative'),
(0,434804,1031013,0,'Forever port alternative'),
(0,434814,1031251,0,'Forever port alternative'),
(0,434815,1031252,0,'Forever port alternative'),
(0,434816,1031254,0,'Forever port alternative'),
(0,434823,1033770,0,'Forever port alternative');

-- 434711 stands on an unpooled Darkfang Lurker; a new one-of-two pool for that spot.
DELETE FROM `pool_template` WHERE `entry`=1400111;
INSERT INTO `pool_template` (`entry`,`max_limit`,`description`) VALUES
(1400111,1,'Dustwallow - Noxious Reaver or Darkfang Lurker');
INSERT INTO `pool_members` (`type`,`spawnId`,`poolSpawnId`,`chance`,`description`) VALUES
(0,434711,1400111,0,'Dustwallow - Noxious Reaver or Darkfang Lurker'),
(0,20033764,1400111,0,'Dustwallow - Noxious Reaver or Darkfang Lurker');

-- Silithus: the vanilla pool spawns one of two Tortured Druid spots, and the Shade of Ambermoon
-- was one of the random creatures for those spots. The port's static Shade sat on one of them.
-- The Shade joins the pool with an equal share, so it still shows up about one time in three.
DELETE FROM `pool_members` WHERE `type`=0 AND `spawnId`=1219900;
UPDATE `pool_members` SET `chance`=0 WHERE `type`=0 AND `poolSpawnId`=1612;
INSERT INTO `pool_members` (`type`,`spawnId`,`poolSpawnId`,`chance`,`description`) VALUES
(0,1219900,1612,0,'Silithus - Shade of Ambermoon');

-- Temple of Ahn'Qiraj: the C'Thun script spawns the Eye of C'Thun itself; the static one stood inside C'Thun.
DELETE FROM `creature_addon` WHERE `guid`=1558900;
DELETE FROM `creature` WHERE `guid`=1558900 AND `id`=15589;
