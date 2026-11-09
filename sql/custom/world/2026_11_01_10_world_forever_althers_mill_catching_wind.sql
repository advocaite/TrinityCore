-- Alther's Mill (98386) and Catching Wind (92840): both could be taken but not finished.
-- Ids cross-checked against the Questie Forever data (Questie team, Questie/QuestieDB, GPLv3):
-- traces/foreverQuestTraces.lua and foreverObjectFixes.lua [673101] Tarantula Egg, Redridge.
--
-- 98386: objective 477504 wants credit 271912 "Tarantula Egg destroyed" x6, but no egg existed in our world.
-- Forever has object 673101 Tarantula Egg at Alther's Mill. Added as a quest-only goober (shows only while
-- the quest is active, gone on use, back after 60 s). Using it gives the 271912 credit.
-- Model 6171 = the vanilla Spider Egg (Zul'Gurub), present in client 70685; the real Forever model id is unknown.
-- Places: Questie's map points converted with the Forever Redridge map bounds (UiMap 1433: x -8575..-10022.9,
-- y -1681.25..-3852.08); only points within 12 yd of one of our ground spawns are used, height taken from
-- those spawns (INFERRED, check in game with .gps that none float or sink).
DELETE FROM `gameobject_template` WHERE `entry`=673101;
INSERT INTO `gameobject_template` (`entry`,`type`,`displayId`,`name`,`IconName`,`castBarCaption`,`unk1`,`size`,`Data0`,`Data1`,`Data2`,`Data3`,`Data4`,`Data5`,`ContentTuningId`,`RequiredLevel`,`AIName`,`ScriptName`,`VerifiedBuild`) VALUES
(673101,10,6171,'Tarantula Egg','','','',1,0,98386,0,3000,0,1,0,0,'SmartGameObjectAI','',0);
DELETE FROM `smart_scripts` WHERE `entryorguid`=673101 AND `source_type`=1;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_param4`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(673101,1,0,0,64,0,100,0,1,0,0,0,0,33,271912,0,0,0,0,0,7,0,0,0,0,0,0,0,0,'Tarantula Egg - On Use - Quest Credit Tarantula Egg destroyed (Alther''s Mill)');
DELETE FROM `gameobject` WHERE `guid` BETWEEN 67310100 AND 67310199;
INSERT INTO `gameobject` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`rotation0`,`rotation1`,`rotation2`,`rotation3`,`spawntimesecs`,`animprogress`,`state`,`ScriptName`,`VerifiedBuild`) VALUES
(67310100,673101,0,44,0,'0',0,0,0,-1,-9165.75,-2686.35,91.22,0.0000,0,0,0.000000,1.000000,60,255,1,'',0),
(67310101,673101,0,44,0,'0',0,0,0,-1,-9216.43,-2688.52,88.81,2.4000,0,0,0.932039,0.362358,60,255,1,'',0),
(67310102,673101,0,44,0,'0',0,0,0,-1,-9162.85,-2690.69,91.00,4.7999,0,0,0.675500,-0.737360,60,255,1,'',0),
(67310103,673101,0,44,0,'0',0,0,0,-1,-9183.12,-2692.86,90.61,0.9167,0,0,0.442469,0.896784,60,255,1,'',0),
(67310104,673101,0,44,0,'0',0,0,0,-1,-9229.46,-2697.20,89.09,3.3167,0,0,0.996170,-0.087442,60,255,1,'',0),
(67310105,673101,0,44,0,'0',0,0,0,-1,-9135.34,-2710.23,92.14,5.7166,0,0,0.279519,-0.960140,60,255,1,'',0),
(67310106,673101,0,44,0,'0',0,0,0,-1,-9120.86,-2729.76,93.80,1.8334,0,0,0.793598,0.608442,60,255,1,'',0),
(67310107,673101,0,44,0,'0',0,0,0,-1,-9128.10,-2734.10,93.81,4.2334,0,0,0.854658,-0.519191,60,255,1,'',0),
(67310108,673101,0,44,0,'0',0,0,0,-1,-9139.69,-2757.98,91.66,0.3501,0,0,0.174157,0.984718,60,255,1,'',0),
(67310109,673101,0,44,0,'0',0,0,0,-1,-9159.96,-2781.86,89.21,2.7501,0,0,0.980903,0.194499,60,255,1,'',0),
(67310110,673101,0,44,0,'0',0,0,0,-1,-9239.59,-2810.08,93.59,5.1501,0,0,0.536718,-0.843762,60,255,1,'',0),
(67310111,673101,0,44,0,'0',0,0,0,-1,-9186.02,-2814.43,93.37,1.2669,0,0,0.591929,0.805990,60,255,1,'',0),
(67310112,673101,0,44,0,'0',0,0,0,-1,-9190.36,-2814.43,93.28,3.6668,0,0,0.965717,-0.259596,60,255,1,'',0),
(67310113,673101,0,44,0,'0',0,0,0,-1,-9187.47,-2820.94,93.32,6.0668,0,0,0.107982,-0.994153,60,255,1,'',0);

-- 92840: Windstone Formation 697118 has lock 4566, which opens only with spell 1320971 "Gather Data" (Open Lock),
-- but the Index Esoteria 254584 has no use effect in the client (no ItemEffect in 70532 or 70685), so nothing can
-- open the stones. The stone itself already casts 1258478 "Data Gathered" (player cast, Kill Credit 253306) when
-- used, so dropping the lock lets a click do it. Still quest-only (Data1 = 92840).
UPDATE `gameobject_template` SET `Data0`=0 WHERE `entry`=697118 AND `Data0`=4566;
