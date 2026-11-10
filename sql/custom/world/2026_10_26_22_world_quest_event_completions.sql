-- Escort and event quests that could be taken but never completed.
-- They carry the completion-event flag. VMaNGOS completed them from scripted map events (command 61/69 success scripts)
-- or gossip scripts (command 7), which the script conversion did not carry over. Each step below is placed where the
-- VMaNGOS success script ran, with its radius, and only completes the quest for players who have it.

-- 836 Rescue OOX-09/HL!: success at the last waypoint (65), within 60 yards; the robot then leaves.
DELETE FROM `smart_scripts` WHERE `entryorguid`=7806 AND `source_type`=0 AND `id` IN (30,31);
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(7806,0,30,31,40,0,100,0,65,820078060,0,0,152,836,0,0,0,0,0,18,60,0,0,0,0,0,0,'Homing Robot OOX-09/HL - On Waypoint 65 - Complete Quest Rescue OOX-09/HL!'),
(7806,0,31,0,61,0,100,0,0,0,0,0,41,3000,0,0,0,0,0,1,0,0,0,0,0,0,0,'Homing Robot OOX-09/HL - On Link - Despawn in 3 s');

-- 6523 Protect Kaya: success at the last waypoint (20), within 50 yards; Kaya then leaves.
DELETE FROM `smart_scripts` WHERE `entryorguid`=11856 AND `source_type`=0 AND `id` IN (30,31);
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(11856,0,30,31,40,0,100,0,20,820118560,0,0,152,6523,0,0,0,0,0,18,50,0,0,0,0,0,0,'Kaya Flathoof - On Waypoint 20 - Complete Quest Protect Kaya'),
(11856,0,31,0,61,0,100,0,0,0,0,0,41,3000,0,0,0,0,0,1,0,0,0,0,0,0,0,'Kaya Flathoof - On Link - Despawn in 3 s');

-- 1173 Challenge Overlord Mok'Morokk: success when he gives up at 30% health.
UPDATE `smart_scripts` SET `link`=30 WHERE `entryorguid`=4500 AND `source_type`=0 AND `id`=2 AND `link`=0;
DELETE FROM `smart_scripts` WHERE `entryorguid`=4500 AND `source_type`=0 AND `id`=30;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(4500,0,30,0,61,0,100,0,0,0,0,0,152,1173,0,0,0,0,0,18,30,0,0,0,0,0,0,'Overlord Mok''Morokk - On Link (gives up) - Complete Quest Challenge Overlord Mok''Morokk');

-- 3367 Suntara Stones: success when Dorius falls at his last waypoint, within 100 yards.
DELETE FROM `smart_scripts` WHERE `entryorguid`=950001100 AND `source_type`=9 AND `id`=8;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(950001100,9,8,0,0,0,100,0,0,0,0,0,152,3367,0,0,0,0,0,18,100,0,0,0,0,0,0,'Dorius Stonetender - Complete Quest Suntara Stones');

-- 5944 In Dreams: Tirion arrives after Taelan falls; VMaNGOS completes the quest at the end of his scene (80 yards).
-- Tirion has no script here, so complete it shortly after he is summoned.
DELETE FROM `smart_scripts` WHERE `entryorguid`=950001157 AND `source_type`=9 AND `id`=6;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(950001157,9,6,0,0,0,100,0,5000,5000,0,0,152,5944,0,0,0,0,0,18,80,0,0,0,0,0,0,'Highlord Taelan Fordring - Complete Quest In Dreams');

-- 4966 Protect Kanati Greycloud: success when the three Galak Assassins die (50 yards). Restore Kanati afterwards,
-- and after the VMaNGOS 180 s limit if the event fails.
DELETE FROM `smart_scripts` WHERE `entryorguid`=10638 AND `source_type`=0 AND `id` BETWEEN 30 AND 39;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(10638,0,30,31,19,0,100,0,4966,0,0,0,63,1,0,1,0,0,0,1,0,0,0,0,0,0,0,'Kanati Greycloud - On Quest Accepted - Reset Counter'),
(10638,0,31,0,61,0,100,0,0,0,0,0,67,1,180000,180000,0,0,100,1,0,0,0,0,0,0,0,'Kanati Greycloud - On Link - Create Timed Event 1 (event time limit)'),
(10638,0,32,0,82,0,100,0,10720,0,0,0,63,1,1,0,0,0,0,1,0,0,0,0,0,0,0,'Kanati Greycloud - On Galak Assassin Died - Add Counter'),
(10638,0,33,34,77,0,100,0,1,3,0,0,152,4966,0,0,0,0,0,18,50,0,0,0,0,0,0,'Kanati Greycloud - On Counter 3 - Complete Quest Protect Kanati Greycloud'),
(10638,0,34,35,61,0,100,0,0,0,0,0,2,0,0,0,0,0,0,1,0,0,0,0,0,0,0,'Kanati Greycloud - On Link - Restore Faction'),
(10638,0,35,0,61,0,100,0,0,0,0,0,82,2,0,0,0,0,0,1,0,0,0,0,0,0,0,'Kanati Greycloud - On Link - Add Quest Giver Flag'),
(10638,0,36,37,59,0,100,0,1,0,0,0,2,0,0,0,0,0,0,1,0,0,0,0,0,0,0,'Kanati Greycloud - On Timed Event 1 - Restore Faction'),
(10638,0,37,0,61,0,100,0,0,0,0,0,82,2,0,0,0,0,0,1,0,0,0,0,0,0,0,'Kanati Greycloud - On Link - Add Quest Giver Flag');

-- 3982 What Is Going On?: success when both waves of four Dark Iron attackers die (80 yards).
DELETE FROM `smart_scripts` WHERE `entryorguid`=9020 AND `source_type`=0 AND `id` BETWEEN 30 AND 39;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(9020,0,30,31,19,0,100,0,3982,0,0,0,63,1,0,1,0,0,0,1,0,0,0,0,0,0,0,'Commander Gor''shak - On Quest Accepted - Reset Counter'),
(9020,0,31,0,61,0,100,0,0,0,0,0,67,1,300000,300000,0,0,100,1,0,0,0,0,0,0,0,'Commander Gor''shak - On Link - Create Timed Event 1 (attackers despawn)'),
(9020,0,32,0,82,0,100,0,0,0,0,0,63,1,1,0,0,0,0,1,0,0,0,0,0,0,0,'Commander Gor''shak - On Summoned Attacker Died - Add Counter'),
(9020,0,33,34,77,0,100,0,1,8,0,0,152,3982,0,0,0,0,0,18,80,0,0,0,0,0,0,'Commander Gor''shak - On Counter 8 - Complete Quest What Is Going On?'),
(9020,0,34,0,61,0,100,0,0,0,0,0,82,2,0,0,0,0,0,1,0,0,0,0,0,0,0,'Commander Gor''shak - On Link - Add Quest Giver Flag'),
(9020,0,35,0,59,0,100,0,1,0,0,0,82,2,0,0,0,0,0,1,0,0,0,0,0,0,0,'Commander Gor''shak - On Timed Event 1 - Add Quest Giver Flag');

-- 3382 A Crew Under Fire: VMaNGOS sends four waves of Spitelash naga (4, 5, 4, 5), each 60 s after the last one is cleared,
-- and completes the quest when the fourth wave dies. Only the first wave was ported. The naga walked from the shore to the
-- crew; here they are summoned where that walk ended.
UPDATE `smart_scripts` SET `target_x`=2373.98,`target_y`=-5960.54,`target_z`=9.625,`target_o`=4.6 WHERE `entryorguid`=950000199 AND `source_type`=9 AND `id` BETWEEN 20 AND 23;
DELETE FROM `smart_scripts` WHERE `entryorguid`=8380 AND `source_type`=0 AND `id` BETWEEN 30 AND 69;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(8380,0,30,31,19,0,100,0,3382,0,0,0,63,1,0,1,0,0,0,1,0,0,0,0,0,0,0,'Captain Vanessa Beltis - On Quest Accepted - Reset Counter'),
(8380,0,31,0,61,0,100,0,0,0,0,0,67,9,600000,600000,0,0,100,1,0,0,0,0,0,0,0,'Captain Vanessa Beltis - On Link - Create Timed Event 9 (event time limit)'),
(8380,0,32,0,82,0,100,0,0,0,0,0,63,1,1,0,0,0,0,1,0,0,0,0,0,0,0,'Captain Vanessa Beltis - On Naga Died - Add Counter'),
(8380,0,33,0,77,0,100,0,1,4,0,0,67,2,60000,60000,0,0,100,1,0,0,0,0,0,0,0,'Captain Vanessa Beltis - On Wave 1 Cleared - Create Timed Event 2'),
(8380,0,34,0,77,0,100,0,1,9,0,0,67,3,60000,60000,0,0,100,1,0,0,0,0,0,0,0,'Captain Vanessa Beltis - On Wave 2 Cleared - Create Timed Event 3'),
(8380,0,35,0,77,0,100,0,1,13,0,0,67,4,60000,60000,0,0,100,1,0,0,0,0,0,0,0,'Captain Vanessa Beltis - On Wave 3 Cleared - Create Timed Event 4'),
(8380,0,36,37,59,0,100,0,2,0,0,0,12,12204,3,180000,0,0,0,8,0,0,0,2373.98,-5960.54,9.625,4.6,'Captain Vanessa Beltis - On Timed Event 2 - Summon Spitelash Raider'),
(8380,0,37,38,61,0,100,0,0,0,0,0,12,12204,3,180000,0,0,0,8,0,0,0,2373.98,-5960.54,9.625,4.6,'Captain Vanessa Beltis - On Link - Summon Spitelash Raider'),
(8380,0,38,39,61,0,100,0,0,0,0,0,12,12204,3,180000,0,0,0,8,0,0,0,2373.98,-5960.54,9.625,4.6,'Captain Vanessa Beltis - On Link - Summon Spitelash Raider'),
(8380,0,39,40,61,0,100,0,0,0,0,0,12,12205,3,180000,0,0,0,8,0,0,0,2373.98,-5960.54,9.625,4.6,'Captain Vanessa Beltis - On Link - Summon Spitelash Witch'),
(8380,0,40,0,61,0,100,0,0,0,0,0,12,12205,3,180000,0,0,0,8,0,0,0,2373.98,-5960.54,9.625,4.6,'Captain Vanessa Beltis - On Link - Summon Spitelash Witch'),
(8380,0,41,42,59,0,100,0,3,0,0,0,12,12204,3,180000,0,0,0,8,0,0,0,2373.98,-5960.54,9.625,4.6,'Captain Vanessa Beltis - On Timed Event 3 - Summon Spitelash Raider'),
(8380,0,42,43,61,0,100,0,0,0,0,0,12,12204,3,180000,0,0,0,8,0,0,0,2373.98,-5960.54,9.625,4.6,'Captain Vanessa Beltis - On Link - Summon Spitelash Raider'),
(8380,0,43,44,61,0,100,0,0,0,0,0,12,12205,3,180000,0,0,0,8,0,0,0,2373.98,-5960.54,9.625,4.6,'Captain Vanessa Beltis - On Link - Summon Spitelash Witch'),
(8380,0,44,0,61,0,100,0,0,0,0,0,12,12205,3,180000,0,0,0,8,0,0,0,2373.98,-5960.54,9.625,4.6,'Captain Vanessa Beltis - On Link - Summon Spitelash Witch'),
(8380,0,45,46,59,0,100,0,4,0,0,0,12,12204,3,180000,0,0,0,8,0,0,0,2373.98,-5960.54,9.625,4.6,'Captain Vanessa Beltis - On Timed Event 4 - Summon Spitelash Raider'),
(8380,0,46,47,61,0,100,0,0,0,0,0,12,12204,3,180000,0,0,0,8,0,0,0,2373.98,-5960.54,9.625,4.6,'Captain Vanessa Beltis - On Link - Summon Spitelash Raider'),
(8380,0,47,48,61,0,100,0,0,0,0,0,12,12205,3,180000,0,0,0,8,0,0,0,2373.98,-5960.54,9.625,4.6,'Captain Vanessa Beltis - On Link - Summon Spitelash Witch'),
(8380,0,48,49,61,0,100,0,0,0,0,0,12,12205,3,180000,0,0,0,8,0,0,0,2373.98,-5960.54,9.625,4.6,'Captain Vanessa Beltis - On Link - Summon Spitelash Witch'),
(8380,0,49,0,61,0,100,0,0,0,0,0,12,12205,3,180000,0,0,0,8,0,0,0,2373.98,-5960.54,9.625,4.6,'Captain Vanessa Beltis - On Link - Summon Spitelash Witch'),
(8380,0,50,51,77,0,100,0,1,18,0,0,152,3382,0,0,0,0,0,18,40,0,0,0,0,0,0,'Captain Vanessa Beltis - On Wave 4 Cleared - Complete Quest A Crew Under Fire'),
(8380,0,51,52,61,0,100,0,0,0,0,0,2,0,0,0,0,0,0,1,0,0,0,0,0,0,0,'Captain Vanessa Beltis - On Link - Restore Faction'),
(8380,0,52,53,61,0,100,0,0,0,0,0,82,2,0,0,0,0,0,1,0,0,0,0,0,0,0,'Captain Vanessa Beltis - On Link - Add Quest Giver Flag'),
(8380,0,53,0,61,0,100,0,0,0,0,0,74,9,0,0,0,0,0,1,0,0,0,0,0,0,0,'Captain Vanessa Beltis - On Link - Remove Timed Event 9'),
(8380,0,54,55,59,0,100,0,9,0,0,0,2,0,0,0,0,0,0,1,0,0,0,0,0,0,0,'Captain Vanessa Beltis - On Timed Event 9 (time limit) - Restore Faction'),
(8380,0,55,0,61,0,100,0,0,0,0,0,82,2,0,0,0,0,0,1,0,0,0,0,0,0,0,'Captain Vanessa Beltis - On Link - Add Quest Giver Flag');

-- 4285 The Northern Pylon / 4287 The Eastern Pylon: examining the pylon completed the quest (VMaNGOS gossip scripts 2177/2178).
UPDATE `gameobject_template` SET `AIName`='SmartGameObjectAI' WHERE `entry` IN (164955,164957) AND `AIName`='' AND `ScriptName`='';
DELETE FROM `smart_scripts` WHERE `entryorguid` IN (164955,164957) AND `source_type`=1 AND `id`=0;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(164955,1,0,0,62,0,100,0,2177,0,0,0,152,4285,0,0,0,0,0,7,0,0,0,0,0,0,0,'Northern Crystal Pylon - On Gossip Option Examine - Complete Quest The Northern Pylon'),
(164957,1,0,0,62,0,100,0,2178,0,0,0,152,4287,0,0,0,0,0,7,0,0,0,0,0,0,0,'Eastern Crystal Pylon - On Gossip Option Examine - Complete Quest The Eastern Pylon');

-- 3821 Dreadmaul Rock: using Sha'ni Proudtusk's Remains summoned her ghost but never completed the quest (VMaNGOS gossip script 2211).
DELETE FROM `smart_scripts` WHERE `entryorguid`=160445 AND `source_type`=1 AND `id`=1;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(160445,1,1,0,64,0,100,0,1,0,0,0,152,3821,0,0,0,0,0,7,0,0,0,0,0,0,0,'Sha''ni Proudtusk''s Remains - On Gossip Hello - Complete Quest Dreadmaul Rock');
