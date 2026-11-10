-- 4288 The Western Pylon (Un'Goro): examining the Western Crystal Pylon never completed the quest. VMaNGOS gossip script 2179
-- completed it; 2026_10_26_22 gave the Northern and Eastern pylons the same step but missed this one.
UPDATE `gameobject_template` SET `AIName`='SmartGameObjectAI' WHERE `entry`=164956 AND `AIName`='' AND `ScriptName`='';
DELETE FROM `smart_scripts` WHERE `entryorguid`=164956 AND `source_type`=1 AND `id`=0;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(164956,1,0,0,62,0,100,0,2179,0,0,0,152,4288,0,0,0,0,0,7,0,0,0,0,0,0,0,'Western Crystal Pylon - On Gossip Option Examine - Complete Quest The Western Pylon');
