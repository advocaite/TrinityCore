-- Classic 1.60: the modern-era NPC import spawned a second copy of three vanilla Orgrimmar NPCs on the exact vanilla
-- coordinates (Mokvar twice in the room with Thrall, plus the two mount props at the Orgrimmar mount vendor).
-- Kept: Mokvar 16012 (vanilla entry, ends several quests), Swift Blue Raptor 14545, Swift Gray Wolf 14541.
-- Dropped: Mokvar 230317, Swift Blue Raptor 276803, Swift Gray Wolf 276805 (import-era duplicates, no quest or vendor links).
DELETE FROM `creature_addon` WHERE `guid` IN (21200548,21200670,21200671);
DELETE FROM `creature` WHERE `guid` IN (21200548,21200670,21200671);
-- rollback:
-- INSERT INTO `creature` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`modelid`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`currentwaypoint`,`MovementType`,`curHealthPct`) VALUES
-- (21200548,230317,1,0,0,'0',0,0,0,-1,0,1,1915.67,-4170.77,41.0061,0.750492,120,0,0,0,NULL),
-- (21200670,276803,1,0,0,'0',0,0,0,-1,0,1,-849.765,-4885.79,21.8995,1.58825,120,0,0,0,NULL),
-- (21200671,276805,1,0,0,'0',0,0,0,-1,0,1,2156.44,-4648.9,50.0618,5.06145,120,0,0,0,NULL);
