-- Quests that could not be taken or handed in because nobody gave or took them.
-- 1) Ahn'Qiraj war effort quests 8793, 8794, 8796 and 8797 had no giver (and 8793/8794 no ender) row. Givers and enders
--    from Questie Forever quest data (https://github.com/Questie/QuestieDB, GPLv3, credit in NOTICE.txt).
-- 2) Two quest NPCs had a template but were never spawned: Pilot Xiggs Fuselighter (Azshara, quests 3449 Arcane Runes
--    and 3461 Return to Tymor) and Hastat the Ancient (Felwood, quest 7634 Ancient Sinew Wrapped Lamina).
--    Positions: Wowhead (Forever) map, turned into world positions with a fit from our own quest givers in the same zone
--    (fit error 3.7 and 5.4 yards). Height: Hastat from an existing spawn about 5 yards away on flat ground; Xiggs from the map tile ground height (11.69). Flags NULL = template.
-- Safe to run again.

INSERT IGNORE INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES
(15703,8793,0),
(15704,8794,0),
(15708,8796,0),
(15709,8797,0);

INSERT IGNORE INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES
(15700,8793,0),
(15700,8794,0);

DELETE FROM `creature` WHERE `guid` IN (47000000, 47000001);
INSERT INTO `creature` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`modelid`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`currentwaypoint`,`curHealthPct`,`MovementType`,`npcflag`,`unit_flags`,`unit_flags2`,`unit_flags3`,`ScriptName`,`StringId`,`VerifiedBuild`) VALUES
(47000000,8392,1,0,0,'0',0,0,0,-1,0,0,2248.6755,-7222.0174,11.70,0,300,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0),
(47000001,14526,1,0,0,'0',0,0,0,-1,0,0,6204.5347,-1131.6248,366.217,0,300,0,0,100,0,NULL,NULL,NULL,NULL,'',NULL,0);
