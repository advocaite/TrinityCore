-- Keeper of the Rolls (16281) takes in 8 Scourge Invasion quests at Light's Hope Chapel (9154 Light's Hope Chapel,
-- 9247 The Keeper's Call, 9295 9299 9300 9301 9302 9304 "... from the Front"). He is set as their quest ender but was never
-- spawned: the VMaNGOS port of the Scourge Invasion event (17) took Commander Thomas Helleran (guid 20067232) and skipped
-- the Keeper standing next to him (VMaNGOS guid 67231 -> 20067231, same numbering as the port). The letters still drop,
-- so players could pick them up and never hand them in. Spawn spot, facing and respawn are VMaNGOS's. Same event as
-- Helleran, so he only shows while the invasion runs. Safe to run again.
DELETE FROM `game_event_creature` WHERE `guid` = 20067231;
DELETE FROM `creature` WHERE `guid` = 20067231;
INSERT INTO `creature` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`modelid`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`currentwaypoint`,`curHealthPct`,`MovementType`,`ScriptName`,`VerifiedBuild`) VALUES
(20067231,16281,0,0,0,'0',0,0,0,-1,0,0,2242.44,-5317.32,82.2506,1.41372,120,0,0,100,0,'',0);
INSERT INTO `game_event_creature` (`eventEntry`,`guid`) VALUES (17,20067231);
