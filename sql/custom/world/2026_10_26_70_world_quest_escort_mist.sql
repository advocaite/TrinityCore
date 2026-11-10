-- 938 Mist (Teldrassil): the escort could be taken but never finished. VMaNGOS made Mist follow the player on accept and
-- completed the quest when she came within 10 yards of Sentinel Arynia Cloudsbreak (3519), failing it after 540 s or when
-- she or the player died. The script conversion kept only the faction and flag changes, so Mist never moved.
-- Mist now follows the player (SMART_ACTION_FOLLOW, event credit 938 to the group at Arynia). At Arynia she wakes the
-- Sentinel's existing reunion script and leaves. If the escort does not arrive within 540 s, the quest fails and Mist despawns
-- (she respawns at her den with the quest-giver flag).
DELETE FROM `smart_scripts` WHERE `entryorguid`=950000074 AND `source_type`=9 AND `id` BETWEEN 2 AND 4;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(950000074,9,2,0,0,0,100,0,0,0,0,0,145,0,0,0,0,0,0,1,0,0,0,0,0,0,0,'Mist - Set Immune To NPC Off (VMaNGOS quest_start_scripts 938)'),
(950000074,9,3,0,0,0,100,0,0,0,0,0,64,1,0,0,0,0,0,7,0,0,0,0,0,0,0,'Mist - Store Escort Player'),
(950000074,9,4,0,0,0,100,0,0,0,0,0,29,0,0,3519,938,1,0,7,0,0,0,0,0,0,0,'Mist - Follow Player to Sentinel Arynia Cloudsbreak (event credit 938)');

DELETE FROM `smart_scripts` WHERE `entryorguid`=950000074 AND `source_type`=9 AND `id`=5;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(950000074,9,5,0,0,0,100,0,0,0,0,0,67,2,540000,540000,0,0,100,1,0,0,0,0,0,0,0,'Mist - Create Timed Event 2 (VMaNGOS 540 s escort limit)');

DELETE FROM `smart_scripts` WHERE `entryorguid`=3568 AND `source_type`=0 AND `id` BETWEEN 2 AND 5;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(3568,0,2,0,65,0,100,0,0,0,0,0,80,356800,2,0,0,0,0,1,0,0,0,0,0,0,0,'Mist - On Follow Complete - Run Reunion Script'),
(3568,0,3,4,59,0,100,0,2,0,0,0,6,938,0,0,0,0,0,12,1,0,0,0,0,0,0,'Mist - On Timed Event 2 (escort too slow) - Fail Quest Mist for the Escort Player'),
(3568,0,4,0,61,0,100,0,0,0,0,0,41,0,0,0,0,0,0,1,0,0,0,0,0,0,0,'Mist - On Link - Despawn'),
(3568,0,5,0,6,0,100,0,0,0,0,0,6,938,0,0,0,0,0,12,1,0,0,0,0,0,0,'Mist - On Death - Fail Quest Mist for the Escort Player');

DELETE FROM `smart_scripts` WHERE `entryorguid`=356800 AND `source_type`=9;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(356800,9,0,0,0,0,100,0,0,0,0,0,45,1,1,0,0,0,0,19,3519,30,0,0,0,0,0,'Mist - Set Data 1 1 on Sentinel Arynia Cloudsbreak (her reunion script)'),
(356800,9,1,0,0,0,100,0,3000,3000,0,0,1,0,0,0,0,0,0,1,0,0,0,0,0,0,0,'Mist - Say Line 0 (leaves into the forest; SmartAI despawns her after the follow)');
