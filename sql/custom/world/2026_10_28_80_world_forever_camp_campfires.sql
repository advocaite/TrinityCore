-- Classic 1.60 (WoW Forever): the Teldrassil and Mulgore adventurer camps have no campfire.
--
-- The Great Outdoors (Teldrassil 96606, Lyreena Duskblade; Mulgore 96605, Kaga Wildhoof) asks to sit by the campfire and gain
-- Boosted Rest, which only a Basic Campfire (529161, spell focus 4) within 15 yards gives. The other four camps have the fire
-- from the Forever sniff (Elwynn, Dun Morogh, Tirisfal 2026_10_01_03, Durotar 2026_10_03_00); these two camps came from the
-- The other Forever server data (2026_10_26_31 adventurer, 2026_10_26_35 tent) without one, so the quest could not be finished and Camping 101:
-- Cooking was never offered (Discord report 2026-10-28). No sniff, the other Forever server or Azeroth row has a fire at either camp.
-- INFERRED: the place. In the sniffed camps the adventurer stands 4-5 yards from the fire, next to the tent; here the fire is
-- 4.5 yards in front of the adventurer (both face east), on her or his own ground height, away from the tent's door.

DELETE FROM `gameobject` WHERE `guid` IN (52916100, 52916101);
INSERT INTO `gameobject` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`,`ScriptName`,`StringId`,`VerifiedBuild`) VALUES
(52916100,529161,1,0,0,'0',0,0,0,-1,9914.89,891.97,1313.19,0.0,0.0,0.0,0.0,1.0,120,255,1,'',NULL,0),   -- Teldrassil, Lyreena Duskblade
(52916101,529161,1,0,0,'0',0,0,0,-1,-2485.43,-364.06,-11.11,0.0,0.0,0.0,0.0,1.0,120,255,1,'',NULL,0); -- Mulgore, Kaga Wildhoof
