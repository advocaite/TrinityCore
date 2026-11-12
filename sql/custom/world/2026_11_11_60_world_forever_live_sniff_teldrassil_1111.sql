-- WoW Forever live realm (client 70888): a new night elf hunter in Shadowglen and Dolanaar, Teldrassil. Capture
-- dump_1.60.1.70888_2026-11-11_21-08-12.pkt (Russian client). Only what our data lacks or has as a guess. Every row here was
-- seen in that capture, except chances marked INFERRED. Safe to run more than once.
-- New gameobject guids 43100300-43100399, new pools 1400400-1400499.
DELETE FROM `pool_members` WHERE `type` = 1 AND `spawnId` BETWEEN 43100300 AND 43100399;
DELETE FROM `pool_members` WHERE `type` = 2 AND (`spawnId` BETWEEN 1400400 AND 1400499 OR `poolSpawnId` BETWEEN 1400400 AND 1400499);
DELETE FROM `pool_template` WHERE `entry` BETWEEN 1400400 AND 1400499;
DELETE FROM `gameobject` WHERE `guid` BETWEEN 43100300 AND 43100399;

-- 1. Shadowglen starter herbs. Live showed Wilted Peacebloom and Stunted Silverleaf at 48 places; we had 8 of them (no pool).
--    Each place is a small pool with the herb(s) live showed there (both herbs share some places, then one of the two comes up).
--    All places are in pool 1400400 with 24 up at once (INFERRED: about half, as in the Eastern Kingdoms starter pools 1400102/1400103).
--    Our 8 nodes (guids 21202280,21202281,21202282,21202283,21202284,21202285,21202286,21202299) join the pool, so they no longer stay up all the time.
DELETE FROM `pool_members` WHERE `type` = 1 AND `spawnId` IN (21202280,21202281,21202282,21202283,21202284,21202285,21202286,21202299);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`,`ScriptName`,`StringId`,`VerifiedBuild`) VALUES
(43100300,656160,1,0,0,'0',0,0,0,-1,10295.6689,1017.9657,1341.0441,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100301,656160,1,0,0,'0',0,0,0,-1,10228.335,971.5261,1353.0209,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100302,656160,1,0,0,'0',0,0,0,-1,10466.9482,762.2379,1317.5619,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100303,656160,1,0,0,'0',0,0,0,-1,10349.6025,633.368,1328.1981,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100304,656160,1,0,0,'0',0,0,0,-1,10281.4404,630.3386,1350.9323,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100305,656160,1,0,0,'0',0,0,0,-1,10350.1406,886.5469,1327.6256,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100306,656160,1,0,0,'0',0,0,0,-1,10422.1758,819.3143,1318.8,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100307,656161,1,0,0,'0',0,0,0,-1,10422.1758,819.3143,1318.8,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100308,656160,1,0,0,'0',0,0,0,-1,10538.8135,690.2952,1320.1113,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100309,656160,1,0,0,'0',0,0,0,-1,10544.7158,599.0226,1334.5956,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100310,656160,1,0,0,'0',0,0,0,-1,10333.0605,1048.4028,1339.3197,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100311,656160,1,0,0,'0',0,0,0,-1,10335.376,939.0938,1328.2877,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100312,656160,1,0,0,'0',0,0,0,-1,10356.5986,1047.901,1343.9575,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100313,656160,1,0,0,'0',0,0,0,-1,10266.3379,756.4011,1345.7289,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100314,656160,1,0,0,'0',0,0,0,-1,10229.7734,684.8489,1358.1642,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100315,656161,1,0,0,'0',0,0,0,-1,10229.7734,684.8489,1358.1642,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100316,656160,1,0,0,'0',0,0,0,-1,10546.3018,607.1702,1333.5912,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100317,656160,1,0,0,'0',0,0,0,-1,10571.8418,907.2136,1311.6553,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100318,656160,1,0,0,'0',0,0,0,-1,10309.29,744.4445,1330.6688,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100319,656160,1,0,0,'0',0,0,0,-1,10600.4072,991.1024,1327.3518,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100320,656160,1,0,0,'0',0,0,0,-1,10713.2783,1042.7587,1352.1691,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100321,656160,1,0,0,'0',0,0,0,-1,10670.8096,689.3438,1329.8235,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100322,656161,1,0,0,'0',0,0,0,-1,10379.9424,891.1441,1326.3512,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100323,656160,1,0,0,'0',0,0,0,-1,10457.3721,891.9097,1315.1537,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100324,656161,1,0,0,'0',0,0,0,-1,10457.3721,891.9097,1315.1537,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100325,656160,1,0,0,'0',0,0,0,-1,10673.873,671.5208,1332.4115,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100326,656161,1,0,0,'0',0,0,0,-1,10269.2881,784.3698,1344.3219,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100327,656161,1,0,0,'0',0,0,0,-1,10323.1494,905.7726,1329.2421,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100328,656161,1,0,0,'0',0,0,0,-1,10432.3594,588.6476,1324.2825,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100329,656161,1,0,0,'0',0,0,0,-1,10446.3252,590.5469,1333.3268,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100330,656161,1,0,0,'0',0,0,0,-1,10474.4912,580.6545,1335.2455,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100331,656161,1,0,0,'0',0,0,0,-1,10267.0635,1006.6823,1346.0709,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100332,656161,1,0,0,'0',0,0,0,-1,10227.126,984.8557,1354.6041,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100333,656161,1,0,0,'0',0,0,0,-1,10330.5752,996.993,1335.359,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100334,656161,1,0,0,'0',0,0,0,-1,10473.4258,1037.8368,1326.1897,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100335,656161,1,0,0,'0',0,0,0,-1,10466.7109,1074.9635,1336.4142,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100336,656161,1,0,0,'0',0,0,0,-1,10235.6221,809.5851,1352.7433,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100337,656161,1,0,0,'0',0,0,0,-1,10429.8408,717.2083,1320.9681,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100338,656161,1,0,0,'0',0,0,0,-1,10454.292,606.8264,1329.5378,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100339,656161,1,0,0,'0',0,0,0,-1,10587.1357,856.493,1307.6501,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100340,656161,1,0,0,'0',0,0,0,-1,10253.4502,992.2795,1344.4888,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100341,656161,1,0,0,'0',0,0,0,-1,10477.3057,641.9809,1326.9342,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100342,656161,1,0,0,'0',0,0,0,-1,10350.1777,709.8924,1327.7197,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100343,656161,1,0,0,'0',0,0,0,-1,10323.0127,790.6875,1328.5585,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100344,656161,1,0,0,'0',0,0,0,-1,10424.1924,1040.0209,1335.6338,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100345,656161,1,0,0,'0',0,0,0,-1,10727.4199,774.2274,1327.5884,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100346,656161,1,0,0,'0',0,0,0,-1,10688.8789,690.0781,1331.5873,0,0,0,0,1,120,255,1,'',NULL,70888),
(43100347,668482,1,0,0,'0',0,0,0,-1,10505.7139,1056.507,1325.0361,5.375,-0.1176,-0.2074,-0.4292,0.8712,120,255,1,'',NULL,70888),
(43100348,668482,1,0,0,'0',0,0,0,-1,10474.5928,1011.6268,1324.2056,0.0655,-0.0069,-0.2383,0.0282,0.9708,120,255,1,'',NULL,70888),
(43100349,668493,1,0,0,'0',0,0,0,-1,10348.3389,1041.0851,1340.7438,0.6878,-0.0674,0.0874,0.3385,0.9345,120,255,1,'',NULL,70888),
(43100350,668493,1,0,0,'0',0,0,0,-1,10492.4668,1052.8854,1325.4763,0.1961,0,0,0.0979,0.9952,120,255,1,'',NULL,70888),
(43100351,2843,1,0,0,'0',0,0,0,-1,10261.8379,960.933,1340.9469,3.2463,0,0,-0.9986,0.0524,300,100,1,'',NULL,70888);
INSERT INTO `pool_template` (`entry`,`max_limit`,`description`) VALUES
(1400400,24,'Wilted Peacebloom and Stunted Silverleaf in Shadowglen'),
(1400401,1,'Shadowglen starter herb place'),
(1400402,1,'Shadowglen starter herb place'),
(1400403,1,'Shadowglen starter herb place'),
(1400404,1,'Shadowglen starter herb place'),
(1400405,1,'Shadowglen starter herb place'),
(1400406,1,'Shadowglen starter herb place'),
(1400407,1,'Shadowglen starter herb place'),
(1400408,1,'Shadowglen starter herb place'),
(1400409,1,'Shadowglen starter herb place'),
(1400410,1,'Shadowglen starter herb place'),
(1400411,1,'Shadowglen starter herb place'),
(1400412,1,'Shadowglen starter herb place'),
(1400413,1,'Shadowglen starter herb place'),
(1400414,1,'Shadowglen starter herb place'),
(1400415,1,'Shadowglen starter herb place'),
(1400416,1,'Shadowglen starter herb place'),
(1400417,1,'Shadowglen starter herb place'),
(1400418,1,'Shadowglen starter herb place'),
(1400419,1,'Shadowglen starter herb place'),
(1400420,1,'Shadowglen starter herb place'),
(1400421,1,'Shadowglen starter herb place'),
(1400422,1,'Shadowglen starter herb place'),
(1400423,1,'Shadowglen starter herb place'),
(1400424,1,'Shadowglen starter herb place'),
(1400425,1,'Shadowglen starter herb place'),
(1400426,1,'Shadowglen starter herb place'),
(1400427,1,'Shadowglen starter herb place'),
(1400428,1,'Shadowglen starter herb place'),
(1400429,1,'Shadowglen starter herb place'),
(1400430,1,'Shadowglen starter herb place'),
(1400431,1,'Shadowglen starter herb place'),
(1400432,1,'Shadowglen starter herb place'),
(1400433,1,'Shadowglen starter herb place'),
(1400434,1,'Shadowglen starter herb place'),
(1400435,1,'Shadowglen starter herb place'),
(1400436,1,'Shadowglen starter herb place'),
(1400437,1,'Shadowglen starter herb place'),
(1400438,1,'Shadowglen starter herb place'),
(1400439,1,'Shadowglen starter herb place'),
(1400440,1,'Shadowglen starter herb place'),
(1400441,1,'Shadowglen starter herb place'),
(1400442,1,'Shadowglen starter herb place'),
(1400443,1,'Shadowglen starter herb place'),
(1400444,1,'Shadowglen starter herb place'),
(1400445,1,'Shadowglen starter herb place'),
(1400446,1,'Shadowglen starter herb place'),
(1400447,1,'Shadowglen starter herb place'),
(1400448,1,'Shadowglen starter herb place');
INSERT INTO `pool_members` (`type`,`spawnId`,`poolSpawnId`,`chance`,`description`) VALUES
(2,1400401,1400400,0,'Shadowglen starter herb place'),
(1,43100300,1400401,0,'Wilted Peacebloom'),
(1,21202299,1400401,0,'Stunted Silverleaf'),
(2,1400402,1400400,0,'Shadowglen starter herb place'),
(1,43100301,1400402,0,'Wilted Peacebloom'),
(2,1400403,1400400,0,'Shadowglen starter herb place'),
(1,21202284,1400403,0,'Wilted Peacebloom'),
(2,1400404,1400400,0,'Shadowglen starter herb place'),
(1,43100302,1400404,0,'Wilted Peacebloom'),
(2,1400405,1400400,0,'Shadowglen starter herb place'),
(1,43100303,1400405,0,'Wilted Peacebloom'),
(2,1400406,1400400,0,'Shadowglen starter herb place'),
(1,43100304,1400406,0,'Wilted Peacebloom'),
(2,1400407,1400400,0,'Shadowglen starter herb place'),
(1,43100305,1400407,0,'Wilted Peacebloom'),
(2,1400408,1400400,0,'Shadowglen starter herb place'),
(1,43100306,1400408,0,'Wilted Peacebloom'),
(1,43100307,1400408,0,'Stunted Silverleaf'),
(2,1400409,1400400,0,'Shadowglen starter herb place'),
(1,43100308,1400409,0,'Wilted Peacebloom'),
(2,1400410,1400400,0,'Shadowglen starter herb place'),
(1,43100309,1400410,0,'Wilted Peacebloom'),
(2,1400411,1400400,0,'Shadowglen starter herb place'),
(1,43100310,1400411,0,'Wilted Peacebloom'),
(2,1400412,1400400,0,'Shadowglen starter herb place'),
(1,21202282,1400412,0,'Wilted Peacebloom'),
(2,1400413,1400400,0,'Shadowglen starter herb place'),
(1,43100311,1400413,0,'Wilted Peacebloom'),
(2,1400414,1400400,0,'Shadowglen starter herb place'),
(1,43100312,1400414,0,'Wilted Peacebloom'),
(2,1400415,1400400,0,'Shadowglen starter herb place'),
(1,43100313,1400415,0,'Wilted Peacebloom'),
(2,1400416,1400400,0,'Shadowglen starter herb place'),
(1,43100314,1400416,0,'Wilted Peacebloom'),
(1,43100315,1400416,0,'Stunted Silverleaf'),
(2,1400417,1400400,0,'Shadowglen starter herb place'),
(1,43100316,1400417,0,'Wilted Peacebloom'),
(2,1400418,1400400,0,'Shadowglen starter herb place'),
(1,43100317,1400418,0,'Wilted Peacebloom'),
(2,1400419,1400400,0,'Shadowglen starter herb place'),
(1,43100318,1400419,0,'Wilted Peacebloom'),
(2,1400420,1400400,0,'Shadowglen starter herb place'),
(1,43100319,1400420,0,'Wilted Peacebloom'),
(2,1400421,1400400,0,'Shadowglen starter herb place'),
(1,43100320,1400421,0,'Wilted Peacebloom'),
(2,1400422,1400400,0,'Shadowglen starter herb place'),
(1,43100321,1400422,0,'Wilted Peacebloom'),
(2,1400423,1400400,0,'Shadowglen starter herb place'),
(1,21202281,1400423,0,'Wilted Peacebloom'),
(1,43100322,1400423,0,'Stunted Silverleaf'),
(2,1400424,1400400,0,'Shadowglen starter herb place'),
(1,43100323,1400424,0,'Wilted Peacebloom'),
(1,43100324,1400424,0,'Stunted Silverleaf'),
(2,1400425,1400400,0,'Shadowglen starter herb place'),
(1,43100325,1400425,0,'Wilted Peacebloom'),
(2,1400426,1400400,0,'Shadowglen starter herb place'),
(1,43100326,1400426,0,'Stunted Silverleaf'),
(2,1400427,1400400,0,'Shadowglen starter herb place'),
(1,43100327,1400427,0,'Stunted Silverleaf'),
(2,1400428,1400400,0,'Shadowglen starter herb place'),
(1,43100328,1400428,0,'Stunted Silverleaf'),
(2,1400429,1400400,0,'Shadowglen starter herb place'),
(1,43100329,1400429,0,'Stunted Silverleaf'),
(2,1400430,1400400,0,'Shadowglen starter herb place'),
(1,43100330,1400430,0,'Stunted Silverleaf'),
(2,1400431,1400400,0,'Shadowglen starter herb place'),
(1,43100331,1400431,0,'Stunted Silverleaf'),
(2,1400432,1400400,0,'Shadowglen starter herb place'),
(1,43100332,1400432,0,'Stunted Silverleaf'),
(2,1400433,1400400,0,'Shadowglen starter herb place'),
(1,43100333,1400433,0,'Stunted Silverleaf'),
(2,1400434,1400400,0,'Shadowglen starter herb place'),
(1,43100334,1400434,0,'Stunted Silverleaf'),
(2,1400435,1400400,0,'Shadowglen starter herb place'),
(1,43100335,1400435,0,'Stunted Silverleaf'),
(2,1400436,1400400,0,'Shadowglen starter herb place'),
(1,21202280,1400436,0,'Wilted Peacebloom'),
(1,43100336,1400436,0,'Stunted Silverleaf'),
(2,1400437,1400400,0,'Shadowglen starter herb place'),
(1,43100337,1400437,0,'Stunted Silverleaf'),
(2,1400438,1400400,0,'Shadowglen starter herb place'),
(1,43100338,1400438,0,'Stunted Silverleaf'),
(2,1400439,1400400,0,'Shadowglen starter herb place'),
(1,43100339,1400439,0,'Stunted Silverleaf'),
(2,1400440,1400400,0,'Shadowglen starter herb place'),
(1,21202285,1400440,0,'Wilted Peacebloom'),
(1,43100340,1400440,0,'Stunted Silverleaf'),
(2,1400441,1400400,0,'Shadowglen starter herb place'),
(1,43100341,1400441,0,'Stunted Silverleaf'),
(2,1400442,1400400,0,'Shadowglen starter herb place'),
(1,43100342,1400442,0,'Stunted Silverleaf'),
(2,1400443,1400400,0,'Shadowglen starter herb place'),
(1,43100343,1400443,0,'Stunted Silverleaf'),
(2,1400444,1400400,0,'Shadowglen starter herb place'),
(1,43100344,1400444,0,'Stunted Silverleaf'),
(2,1400445,1400400,0,'Shadowglen starter herb place'),
(1,43100345,1400445,0,'Stunted Silverleaf'),
(2,1400446,1400400,0,'Shadowglen starter herb place'),
(1,43100346,1400446,0,'Stunted Silverleaf'),
(2,1400447,1400400,0,'Shadowglen starter herb place'),
(1,21202283,1400447,0,'Wilted Peacebloom'),
(2,1400448,1400400,0,'Shadowglen starter herb place'),
(1,21202286,1400448,0,'Wilted Peacebloom'),
(1,43100351,20006,0,'Battered Chest (live place)');

-- 2. Gnarlpine Totems (quest Nature's Call 97977). Ours had guessed places (VerifiedBuild 0) a few yards off.
--    A guessed totem within 15 yd of a live place moves onto it; live places with no totem of ours within 15 yd are added.
UPDATE `gameobject` SET `position_x` = 10493.8076, `position_y` = 947.4827, `position_z` = 1318.8965, `orientation` = 3.8671, `rotation0` = 0.1842, `rotation1` = 0.0907, `rotation2` = -0.9136, `rotation3` = 0.3511, `VerifiedBuild` = 70888 WHERE `guid` = 9518420 AND `id` = 668482; -- was 5.6 yd off
UPDATE `gameobject` SET `position_x` = 10333.6377, `position_y` = 1041.6267, `position_z` = 1339.5436, `orientation` = 2.0553, `rotation0` = 0, `rotation1` = 0, `rotation2` = 0.8561, `rotation3` = 0.5169, `VerifiedBuild` = 70888 WHERE `guid` = 9518416 AND `id` = 668482; -- was 2.2 yd off
UPDATE `gameobject` SET `position_x` = 10488.1914, `position_y` = 1011.5417, `position_z` = 1324.4697, `orientation` = 5.5255, `rotation0` = 0, `rotation1` = 0, `rotation2` = -0.3698, `rotation3` = 0.9291, `VerifiedBuild` = 70888 WHERE `guid` = 9518417 AND `id` = 668482; -- was 5.0 yd off
-- new live place for 668482: guid 43100347
UPDATE `gameobject` SET `position_x` = 10491.6621, `position_y` = 1060.0226, `position_z` = 1325.4705, `orientation` = 5.0192, `rotation0` = 0, `rotation1` = 0, `rotation2` = -0.5908, `rotation3` = 0.8068, `VerifiedBuild` = 70888 WHERE `guid` = 9518415 AND `id` = 668482; -- was 5.7 yd off
-- new live place for 668482: guid 43100348
-- new live place for 668493: guid 43100349
-- new live place for 668493: guid 43100350
UPDATE `gameobject` SET `position_x` = 10535.1719, `position_y` = 1068.3125, `position_z` = 1320.9443, `orientation` = 4.1277, `rotation0` = 0, `rotation1` = 0, `rotation2` = -0.8809, `rotation3` = 0.4733, `VerifiedBuild` = 70888 WHERE `guid` = 9518421 AND `id` = 668493; -- was 5.0 yd off
UPDATE `gameobject` SET `position_x` = 10559.54, `position_y` = 989.6597, `position_z` = 1318.257, `orientation` = 3.5054, `rotation0` = -0.0955, `rotation1` = -0.0546, `rotation2` = -0.9768, `rotation3` = 0.1835, `VerifiedBuild` = 70888 WHERE `guid` = 9518424 AND `id` = 668493; -- was 4.5 yd off

-- 3. Battered Chest: one more live place for the Shadowglen chest pool 20006 (it keeps its limit of 1).
-- guid 43100351

-- 4. Lyrai (General Supplies, Dolanaar) also sells Skinning Knife and Mining Pick on live, in slots 7 and 8.
DELETE FROM `npc_vendor` WHERE `entry` = 3587 AND `item` IN (7005,2901);
INSERT INTO `npc_vendor` (`entry`,`slot`,`item`,`maxcount`,`incrtime`,`ExtendedCost`,`type`,`BonusListIDs`,`PlayerConditionID`,`IgnoreFiltering`,`VerifiedBuild`) VALUES
(3587,7,7005,0,0,0,1,'',0,0,70888),
(3587,8,2901,0,0,0,1,'',0,0,70888);

-- 5. Fanorran Stilloak (Junior Mining Trainer, Dolanaar): live npcflag 211 (adds quest giver and profession trainer) and the
--    junior trainer menu 44733 ("Train me." / "I would like to buy from you."), as Walter Mason and Vartha Rockmane have.
UPDATE `creature_template` SET `npcflag` = 211 WHERE `entry` = 267334;
DELETE FROM `creature_template_gossip` WHERE `CreatureID` = 267334;
INSERT INTO `creature_template_gossip` (`CreatureID`,`MenuID`,`VerifiedBuild`) VALUES (267334,44733,70888);

-- 6. Tools in hand for the three Dolanaar junior trainers (ours had none). INSERT IGNORE keeps any row we have.
INSERT IGNORE INTO `creature_equip_template` (`CreatureID`,`ID`,`ItemID1`,`AppearanceModID1`,`ItemVisual1`,`ItemID2`,`AppearanceModID2`,`ItemVisual2`,`ItemID3`,`AppearanceModID3`,`ItemVisual3`,`VerifiedBuild`) VALUES
(267333,1,7005,0,0,0,0,0,0,0,0,70888),
(267334,1,1910,0,0,0,0,0,0,0,0,70888),
(267335,1,2706,0,0,0,0,0,0,0,0,70888);
UPDATE `creature` SET `equipment_id` = 1 WHERE `id` IN (267333,267334,267335) AND `equipment_id` = 0;

-- 7. Drops live gave that ours never does. Chances are INFERRED from small samples: Ruined Leather Scraps 2934 dropped 4 times
--    in 14 kills of Young Thistle Boars and Mangy Nightsabers (25%), Linen Cloth 2589 2 times in 7 Grellkin kills (20%).
DELETE FROM `creature_loot_template` WHERE (`Entry`,`Item`) IN ((1984,2934),(1985,2934),(1989,2589));
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(1984,0,2934,25,0,1,0,1,1,'live 70888 capture, chance inferred'),
(1985,0,2934,25,0,1,0,1,1,'live 70888 capture, chance inferred'),
(1989,0,2589,20,0,1,0,1,1,'live 70888 capture, chance inferred');

-- 8. Fang of Githyiss (97236) and Nature's Call (97977): live files them under Shadowglen (188) and lets any race take them.
UPDATE `quest_template` SET `QuestSortID` = 188, `AllowableRaces` = 18446744073709551615, `VerifiedBuild` = 70888 WHERE `ID` IN (97236,97977);

-- 9. Russian texts we lacked (only where no ruRU row exists).
INSERT IGNORE INTO `quest_offer_reward_locale` (`ID`,`locale`,`RewardText`,`VerifiedBuild`) VALUES (97977,'ruRU','Все даже хуже, чем я думала.\n\nЭти выгравированные изображения повествуют о том, что фурболгам из племени Кривой Сосны начали сниться кошмары о том, как им приносит погибель какая-то огромная тень. Вскоре этот страх превратился в ярость – а тень приняла облик их бывших друзей-эльфов. Как же жутко постепенно лишаться рассудка, осознавая это...\n\nНадеяться теперь приходится только на то, что нам удастся лучше понять ситуацию на Тельдрассиле. Спасибо тебе за помощь, $n.',70888);
INSERT IGNORE INTO `quest_offer_reward_locale` (`ID`,`locale`,`RewardText`,`VerifiedBuild`) VALUES (97979,'ruRU','Ты $gвидел:видела; дары богини. Пользуйся ими с умом – и она никогда тебя не оставит.',70888);
INSERT IGNORE INTO `quest_request_items_locale` (`ID`,`locale`,`CompletionText`,`VerifiedBuild`) VALUES (458,'ruRU','Помоги мне подтвердить или опровергнуть мои подозрения, $n. Принеси мне 8 кусочков мха Скверны.',70888);
INSERT IGNORE INTO `quest_request_items_locale` (`ID`,`locale`,`CompletionText`,`VerifiedBuild`) VALUES (97977,'ruRU','Не обращай внимание на большие тотемы, которые тебе могут встретиться. Я все равно не смогу с ними ничего сделать, даже если ты сумеешь их ко мне притащить!',70888);
