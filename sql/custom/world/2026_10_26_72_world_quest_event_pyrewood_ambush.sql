-- 452 Pyrewood Ambush (Silverpine): only the first councilman ever came, and the quest could not be finished. VMaNGOS ran it as a
-- 600 s map event: four waves walk in from the council door, each 5 s after the last one is all dead - Smithers; Thatcher and
-- Hendricks; Hartin, Higarth and Wilhelm; Lord Mayor Morrison, Cooper and Brunswick - and the quest completes for players
-- within 60 yards when the last wave dies. It failed when Deathstalker Faerleia died or the time ran out. The conversion kept
-- the first summon only; the councilmen's own wave scripts were never converted.
-- Faerleia now runs the waves herself: every councilman she summons attacks her on arrival, each death adds to a counter,
-- and counters 1, 3 and 6 call the next wave. Counter 9 completes the quest and Faerleia thanks the group (VMaNGOS text 545).

DELETE FROM `creature_text` WHERE `CreatureID`=2058 AND `GroupID`=1;
INSERT INTO `creature_text` (`CreatureID`,`GroupID`,`ID`,`Text`,`Type`,`Language`,`Probability`,`Emote`,`Duration`,`Sound`,`SoundPlayType`,`BroadcastTextId`,`TextRange`,`comment`) VALUES
(2058,1,0,'Well done. A blow to Arugal no doubt!',12,0,100,0,0,0,0,545,0,'Deathstalker Faerleia - Pyrewood Ambush completed (VMaNGOS)');

-- On accept (actionlist 950000020 already: faction 232, no quest-giver flag, ambush line, Smithers at 8 s): reset the counter,
-- remember the player, and start the 600 s limit.
DELETE FROM `smart_scripts` WHERE `entryorguid`=950000020 AND `source_type`=9 AND `id` BETWEEN 4 AND 6;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(950000020,9,4,0,0,0,100,0,0,0,0,0,63,1,0,1,0,0,0,1,0,0,0,0,0,0,0,'Deathstalker Faerleia - Reset Councilman Death Counter'),
(950000020,9,5,0,0,0,100,0,0,0,0,0,64,1,0,0,0,0,0,7,0,0,0,0,0,0,0,'Deathstalker Faerleia - Store Quest Player'),
(950000020,9,6,0,0,0,100,0,0,0,0,0,67,2,600000,600000,0,0,100,1,0,0,0,0,0,0,0,'Deathstalker Faerleia - Create Timed Event 2 (VMaNGOS 600 s event limit)');

DELETE FROM `smart_scripts` WHERE `entryorguid`=2058 AND `source_type`=0 AND `id` BETWEEN 3 AND 12;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(2058,0,3,0,82,0,100,0,0,0,0,0,63,1,1,0,0,0,0,1,0,0,0,0,0,0,0,'Deathstalker Faerleia - On Summoned Councilman Died - Add 1 to Counter'),
(2058,0,4,0,77,0,100,0,1,1,0,0,80,205800,2,0,0,0,0,1,0,0,0,0,0,0,0,'Deathstalker Faerleia - On Counter 1 - Wave 2 (Thatcher, Hendricks)'),
(2058,0,5,0,77,0,100,0,1,3,0,0,80,205801,2,0,0,0,0,1,0,0,0,0,0,0,0,'Deathstalker Faerleia - On Counter 3 - Wave 3 (Hartin, Higarth, Wilhelm)'),
(2058,0,6,0,77,0,100,0,1,6,0,0,80,205802,2,0,0,0,0,1,0,0,0,0,0,0,0,'Deathstalker Faerleia - On Counter 6 - Wave 4 (Lord Mayor Morrison, Cooper, Brunswick)'),
(2058,0,7,0,77,0,100,0,1,9,0,0,80,205803,2,0,0,0,0,1,0,0,0,0,0,0,0,'Deathstalker Faerleia - On Counter 9 - Ambush Done'),
(2058,0,8,9,59,0,100,0,2,0,0,0,6,452,0,0,0,0,0,12,1,0,0,0,0,0,0,'Deathstalker Faerleia - On Timed Event 2 (600 s) - Fail Quest Pyrewood Ambush'),
(2058,0,9,0,61,0,100,0,0,0,0,0,41,0,0,0,0,0,0,1,0,0,0,0,0,0,0,'Deathstalker Faerleia - On Link - Despawn (respawns with the quest)'),
(2058,0,10,0,6,0,100,0,0,0,0,0,6,452,0,0,0,0,0,12,1,0,0,0,0,0,0,'Deathstalker Faerleia - On Death - Fail Quest Pyrewood Ambush');

-- Waves, 5 s after the last councilman of the wave before fell, at the VMaNGOS summon spots by the council door.
DELETE FROM `smart_scripts` WHERE `entryorguid` IN (205800,205801,205802,205803) AND `source_type`=9;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(205800,9,0,0,0,0,100,0,5000,5000,0,0,12,2061,3,240000,0,0,0,8,0,0,0,-398.219,1511.08,18.952,4.86947,'Deathstalker Faerleia - Summon Councilman Thatcher'),
(205800,9,1,0,0,0,100,0,0,0,0,0,12,2062,3,240000,0,0,0,8,0,0,0,-396.638,1511.03,18.952,4.66003,'Deathstalker Faerleia - Summon Councilman Hendricks'),
(205801,9,0,0,0,0,100,0,5000,5000,0,0,12,2064,3,240000,0,0,0,8,0,0,0,-397.428,1511.84,18.952,4.69494,'Deathstalker Faerleia - Summon Councilman Hartin'),
(205801,9,1,0,0,0,100,0,0,0,0,0,12,2066,3,240000,0,0,0,8,0,0,0,-398.346,1511.81,18.952,4.7822,'Deathstalker Faerleia - Summon Councilman Higarth'),
(205801,9,2,0,0,0,100,0,0,0,0,0,12,2063,3,240000,0,0,0,8,0,0,0,-396.381,1511.8,18.952,4.66003,'Deathstalker Faerleia - Summon Councilman Wilhelm'),
(205802,9,0,0,0,0,100,0,5000,5000,0,0,12,2068,3,240000,0,0,0,8,0,0,0,-397.366,1512.61,18.952,4.7473,'Deathstalker Faerleia - Summon Lord Mayor Morrison'),
(205802,9,1,0,0,0,100,0,0,0,0,0,12,2067,3,240000,0,0,0,8,0,0,0,-398.522,1512.69,18.952,4.79966,'Deathstalker Faerleia - Summon Councilman Brunswick'),
(205802,9,2,0,0,0,100,0,0,0,0,0,12,2065,3,240000,0,0,0,8,0,0,0,-396.286,1512.55,18.952,4.69494,'Deathstalker Faerleia - Summon Councilman Cooper'),
(205803,9,0,0,0,0,100,0,0,0,0,0,152,452,0,0,0,0,0,18,60,0,0,0,0,0,0,'Deathstalker Faerleia - Complete Quest Pyrewood Ambush (60 yards)'),
(205803,9,1,0,0,0,100,0,0,0,0,0,74,2,0,0,0,0,0,1,0,0,0,0,0,0,0,'Deathstalker Faerleia - Remove Timed Event 2'),
(205803,9,2,0,0,0,100,0,0,0,0,0,2,68,0,0,0,0,0,1,0,0,0,0,0,0,0,'Deathstalker Faerleia - Set Faction to Undercity'),
(205803,9,3,0,0,0,100,0,2000,2000,0,0,1,1,0,0,0,0,0,1,0,0,0,0,0,0,0,'Deathstalker Faerleia - Say Line 1'),
(205803,9,4,0,0,0,100,0,0,0,0,0,82,2,0,0,0,0,0,1,0,0,0,0,0,0,0,'Deathstalker Faerleia - Add Quest-Giver Flag');

-- Every councilman of the ambush attacks Faerleia when he arrives (VMaNGOS walked them in by waypoints, then they fought).
UPDATE `creature_template` SET `AIName`='SmartAI' WHERE `entry` IN (2060,2061,2064,2068) AND `AIName`='' AND `ScriptName`='';
DELETE FROM `smart_scripts` WHERE `entryorguid` IN (2060,2061,2062,2063,2064,2065,2066,2067,2068) AND `source_type`=0 AND `id`=30;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(2060,0,30,0,54,0,100,0,0,0,0,0,49,0,0,0,0,0,0,19,2058,50,0,0,0,0,0,'Councilman Smithers - Just Summoned - Attack Deathstalker Faerleia'),
(2061,0,30,0,54,0,100,0,0,0,0,0,49,0,0,0,0,0,0,19,2058,50,0,0,0,0,0,'Councilman Thatcher - Just Summoned - Attack Deathstalker Faerleia'),
(2062,0,30,0,54,0,100,0,0,0,0,0,49,0,0,0,0,0,0,19,2058,50,0,0,0,0,0,'Councilman Hendricks - Just Summoned - Attack Deathstalker Faerleia'),
(2063,0,30,0,54,0,100,0,0,0,0,0,49,0,0,0,0,0,0,19,2058,50,0,0,0,0,0,'Councilman Wilhelm - Just Summoned - Attack Deathstalker Faerleia'),
(2064,0,30,0,54,0,100,0,0,0,0,0,49,0,0,0,0,0,0,19,2058,50,0,0,0,0,0,'Councilman Hartin - Just Summoned - Attack Deathstalker Faerleia'),
(2065,0,30,0,54,0,100,0,0,0,0,0,49,0,0,0,0,0,0,19,2058,50,0,0,0,0,0,'Councilman Cooper - Just Summoned - Attack Deathstalker Faerleia'),
(2066,0,30,0,54,0,100,0,0,0,0,0,49,0,0,0,0,0,0,19,2058,50,0,0,0,0,0,'Councilman Higarth - Just Summoned - Attack Deathstalker Faerleia'),
(2067,0,30,0,54,0,100,0,0,0,0,0,49,0,0,0,0,0,0,19,2058,50,0,0,0,0,0,'Councilman Brunswick - Just Summoned - Attack Deathstalker Faerleia'),
(2068,0,30,0,54,0,100,0,0,0,0,0,49,0,0,0,0,0,0,19,2058,50,0,0,0,0,0,'Lord Mayor Morrison - Just Summoned - Attack Deathstalker Faerleia');
