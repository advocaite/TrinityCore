-- Seeking Refuge 99144 (Discord, soulkilleer 2026-11-05): Bareth Dawnstone stands up to five times around Crusader's Outpost and
-- the escort never starts. Our sniff import put five copies of him in the world: the sniffs saw the one escort at five points
-- of his walk out of the tower. The quest also had no objective and nothing started his route when the quest is taken.
-- Now: one Bareth, at the foot of the tower (the lowest sniffed spot by the tower; the tower-top spot is left out so he never
-- has to find his way down the stairs); taking the quest starts his walk out along the road, over the spots the sniffs saw
-- him at; the end of the walk gives the escort credit and the quest's completion event (2026_10_28_70, 2026_10_29_00). Safe to run again.

-- One Bareth: drop the four copies seen along the route (and 27595400, an old dev-only spawn), keep the one at the tower foot.
DELETE FROM `creature` WHERE `guid` IN (21200482, 21200483, 21200484, 21200485, 21200486, 27595400);
INSERT INTO `creature` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`modelid`,`equipment_id`,`spawntimesecs`,`wander_distance`,`currentwaypoint`,`curHealthPct`,`MovementType`,`npcflag`,`unit_flags`,`unit_flags2`,`unit_flags3`,`ScriptName`,`VerifiedBuild`) VALUES
(21200486,275954,0,0,0,'0',0,0,0,-1,2455.2324,1598.3251,40.3167,1.973064,0,1,120,0,0,NULL,0,NULL,NULL,NULL,NULL,'',70170);

-- The objective (the ruRU text 2026_10_27_40 already uses this ID).
DELETE FROM `quest_objectives` WHERE `QuestID`=99144;
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES
(91500101,99144,0,0,0,275954,1,0,0,0,0,0,1,'Escort Bareth Dawnstone out of Crusader''s Outpost',0);

-- His walk: the points are the three road spots the sniffs saw him at (exact positions, nothing guessed).
DELETE FROM `waypoint_path` WHERE `PathId`=1100275954;
INSERT INTO `waypoint_path` (`PathId`,`MoveType`,`Flags`,`Velocity`,`Comment`) VALUES
(1100275954,0,0,NULL,'Bareth Dawnstone, Seeking Refuge 99144 escort (sniffed positions)');
DELETE FROM `waypoint_path_node` WHERE `PathId`=1100275954;
INSERT INTO `waypoint_path_node` (`PathId`,`NodeId`,`PositionX`,`PositionY`,`PositionZ`,`Orientation`,`Delay`) VALUES
(1100275954,1,2406.0955,1569.382,31.6949,NULL,0),
(1100275954,2,2402.6711,1552.8057,31.5259,NULL,0),
(1100275954,3,2401.9224,1529.4888,31.8721,NULL,0);

-- Taking the quest: no quest-giver flag while he walks, he says his line and starts the walk.
-- The end of the walk (ids 12-14) is in 2026_10_28_70 and 2026_10_29_00; id 16 adds his goodbye line there.
DELETE FROM `smart_scripts` WHERE `entryorguid`=275954 AND `source_type`=0 AND `id` IN (10, 11, 15, 16);
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_chance`,`event_param1`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`target_type`,`target_param2`,`comment`) VALUES
(275954,0,10,11,19,100,99144,81,0,0,0,0,1,0,'Bareth Dawnstone - On quest 99144 accepted - No quest-giver flag'),
(275954,0,11,15,61,100,0,53,0,1100275954,0,99144,1,0,'Bareth Dawnstone - Linked - Start the walk out (Seeking Refuge 99144)'),
(275954,0,15,0,61,100,0,1,1,0,0,0,1,0,'Bareth Dawnstone - Linked - Say "I believe I''ve overstayed my welcome"'),
(275954,0,16,0,58,100,4294967295,1,4,0,0,0,1,0,'Bareth Dawnstone - Walk ended - Say "I can make it the rest of the way on my own"');
