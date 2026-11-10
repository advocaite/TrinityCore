-- 8746/8762 Metzen the Reindeer: both quests carry the completion-event flag, so after 2026_10_26_21 gave the rescue's kill
-- credit they still waited for the event. VMaNGOS gossip script 6763 also completed both quests at that step (command 7).
-- Metzen's freeing script now does the same before he leaves.
DELETE FROM `smart_scripts` WHERE `entryorguid`=1566400 AND `source_type`=9;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(1566400,9,0,0,0,0,100,0,0,0,0,0,11,25952,0,0,0,0,0,1,0,0,0,0,0,0,0,'Metzen the Reindeer - On Script - Cast ''Reindeer Dust Effect'''),
(1566400,9,1,0,0,0,100,0,0,0,0,0,33,15664,0,0,0,0,0,7,0,0,0,0,0,0,0,'Metzen the Reindeer - On Script - Quest Credit Metzen the Reindeer'),
(1566400,9,2,0,0,0,100,0,0,0,0,0,152,8746,0,0,0,0,0,7,0,0,0,0,0,0,0,'Metzen the Reindeer - On Script - Complete Event of Metzen the Reindeer (Alliance)'),
(1566400,9,3,0,0,0,100,0,0,0,0,0,152,8762,0,0,0,0,0,7,0,0,0,0,0,0,0,'Metzen the Reindeer - On Script - Complete Event of Metzen the Reindeer (Horde)'),
(1566400,9,4,0,0,0,100,0,11000,11000,0,0,41,0,0,0,0,0,0,1,0,0,0,0,0,0,0,'Metzen the Reindeer - On Script - Despawn');
