-- 219 Missing In Action (Redridge): Corporal Keeshan never set off. VMaNGOS started his waypoints (creature_movement_template
-- 349 path 0, here path 810003490) on accept and ended the map event as a success at waypoint 67, completing the quest for
-- players within 60 yards; it failed when he died or the player went further than 60 yards. The conversion kept his texts
-- and waypoint scripts but not the start of the walk nor the success.
-- Keeshan now walks the escort path with quest 219 (SmartAI fails it when he dies or the escort leaves him), and at waypoint
-- 67, after his report to Marshal Marris, the escort ends as a success: every group member in reward range gets the credit.
DELETE FROM `smart_scripts` WHERE `entryorguid`=950000006 AND `source_type`=9 AND `id`=2;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(950000006,9,2,0,0,0,100,0,0,0,0,0,53,0,810003490,0,219,0,0,7,0,0,0,0,0,0,0,'Corporal Keeshan - Start Escort Path (quest 219) with the Player (VMaNGOS quest_start_scripts 219)');

-- Waypoint 67: replace the plain despawn 20 s after the report with the escort's successful end, which despawns him 20 s later.
DELETE FROM `smart_scripts` WHERE `entryorguid`=950001117 AND `source_type`=9 AND `id`=2;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(950001117,9,2,0,0,0,100,0,0,0,0,0,55,20000,219,0,0,0,0,1,0,0,0,0,0,0,0,'Corporal Keeshan - End Escort as a Success (quest 219), Despawn in 20 s (VMaNGOS creature_movement_scripts 34967)');
