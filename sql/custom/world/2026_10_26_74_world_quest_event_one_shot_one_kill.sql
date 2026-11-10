-- 5713 One Shot. One Kill. (Ashenvale): the Blackwood waves and Marosh came, but the quest never completed. VMaNGOS ended the
-- map event as a success when Marosh the Devious died (completing the quest within 60 yards), then Sentinel Aynasha thanked the
-- player, lost her root and ran off along her path; it failed if she died or the 600 s ran out. Marosh already tells her (Set
-- Data 1 1 on death) but she had nothing listening.
-- Aynasha now completes the quest for the group on that signal, plays the VMaNGOS success texts and leaves by her converted path
-- (810117110, despawn at its end; she respawns with the quest). Her death or the time limit fails the quest.

DELETE FROM `creature_text` WHERE `CreatureID`=11711 AND `GroupID` IN (2,3,4);
INSERT INTO `creature_text` (`CreatureID`,`GroupID`,`ID`,`Text`,`Type`,`Language`,`Probability`,`Emote`,`Duration`,`Sound`,`SoundPlayType`,`BroadcastTextId`,`TextRange`,`comment`) VALUES
(11711,2,0,'Praise Elune! I don''t know if I could have survived the day without you, friend.',12,0,100,0,0,0,0,7201,0,'Sentinel Aynasha - One Shot. One Kill. completed (VMaNGOS)'),
(11711,3,0,'My leg feels much better now, the remedy must be working. If you will excuse me, I must go report to my superiors about what has transpired here.',12,0,100,1,0,0,0,7202,0,'Sentinel Aynasha - One Shot. One Kill. completed (VMaNGOS)'),
(11711,4,0,'Please speak with Sentinel Onaeya at Maestra''s Post in Ashenvale, she will see to it that you are properly rewarded for your bravery this day.',12,0,100,1,0,0,0,7328,0,'Sentinel Aynasha - One Shot. One Kill. completed (VMaNGOS)');

-- On accept: remember the player and start the 600 s limit first, then the converted waves (unchanged).
DELETE FROM `smart_scripts` WHERE `entryorguid`=950000245 AND `source_type`=9;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(950000245,9,0,0,0,0,100,0,0,0,0,0,64,1,0,0,0,0,0,7,0,0,0,0,0,0,0,'Sentinel Aynasha - Store Quest Player'),
(950000245,9,1,0,0,0,100,0,0,0,0,0,67,2,600000,600000,0,0,100,1,0,0,0,0,0,0,0,'Sentinel Aynasha - Create Timed Event 2 (VMaNGOS 600 s event limit)'),
(950000245,9,2,0,0,0,100,0,1000,1000,0,0,1,0,0,1,0,0,0,7,0,0,0,0,0,0,0,'VMaNGOS quest_start_scripts 5713'),
(950000245,9,3,0,0,0,100,0,0,0,0,0,83,2,0,0,0,0,0,1,0,0,0,0,0,0,0,'VMaNGOS quest_start_scripts 5713'),
(950000245,9,4,0,0,0,100,0,0,0,0,0,11,18373,32,0,0,0,0,1,0,0,0,0,0,0,0,'VMaNGOS quest_start_scripts 5713'),
(950000245,9,5,0,0,0,100,0,1000,1000,0,0,12,11713,3,60000,0,0,0,8,0,0,0,4390.06,-3.516,68.802,1.03,'VMaNGOS quest_start_scripts 5713'),
(950000245,9,6,0,0,0,100,0,0,0,0,0,12,11713,3,60000,0,0,0,8,0,0,0,4388.57,-4.578,68.786,1.414,'VMaNGOS quest_start_scripts 5713'),
(950000245,9,7,0,0,0,100,0,48000,48000,0,0,12,11713,3,60000,0,0,0,8,0,0,0,4387.61,-3.882,68.539,4.136,'VMaNGOS quest_start_scripts 5713'),
(950000245,9,8,0,0,0,100,0,0,0,0,0,12,11713,3,60000,0,0,0,8,0,0,0,4387.08,-2.346,68.148,1.518,'VMaNGOS quest_start_scripts 5713'),
(950000245,9,9,0,0,0,100,0,0,0,0,0,12,11713,3,60000,0,0,0,8,0,0,0,4388.91,-3.784,68.697,4.171,'VMaNGOS quest_start_scripts 5713'),
(950000245,9,10,0,0,0,100,0,50000,50000,0,0,12,11714,3,60000,0,0,0,8,0,0,0,4385.79,-4.063,68.289,1.082,'VMaNGOS quest_start_scripts 5713'),
(950000245,9,11,0,0,0,100,0,20000,20000,0,0,1,1,0,1,0,0,0,7,0,0,0,0,0,0,0,'VMaNGOS quest_start_scripts 5713');

DELETE FROM `smart_scripts` WHERE `entryorguid`=11711 AND `source_type`=0 AND `id` BETWEEN 10 AND 13;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(11711,0,10,0,38,0,100,0,1,1,0,0,80,1171100,2,0,0,0,0,1,0,0,0,0,0,0,0,'Sentinel Aynasha - On Data Set 1 1 (Marosh died) - Event Success'),
(11711,0,11,12,59,0,100,0,2,0,0,0,6,5713,0,0,0,0,0,12,1,0,0,0,0,0,0,'Sentinel Aynasha - On Timed Event 2 (600 s) - Fail Quest One Shot. One Kill.'),
(11711,0,12,0,61,0,100,0,0,0,0,0,41,0,0,0,0,0,0,1,0,0,0,0,0,0,0,'Sentinel Aynasha - On Link - Despawn (VMaNGOS generic_scripts 571301)'),
(11711,0,13,0,6,0,100,0,0,0,0,0,6,5713,0,0,0,0,0,12,1,0,0,0,0,0,0,'Sentinel Aynasha - On Death - Fail Quest One Shot. One Kill.');

DELETE FROM `smart_scripts` WHERE `entryorguid`=1171100 AND `source_type`=9;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(1171100,9,0,0,0,0,100,0,0,0,0,0,152,5713,0,0,0,0,0,18,60,0,0,0,0,0,0,'Sentinel Aynasha - Complete Quest One Shot. One Kill. (60 yards; VMaNGOS generic_scripts 571302)'),
(1171100,9,1,0,0,0,100,0,0,0,0,0,74,2,0,0,0,0,0,1,0,0,0,0,0,0,0,'Sentinel Aynasha - Remove Timed Event 2'),
(1171100,9,2,0,0,0,100,0,1000,1000,0,0,1,2,0,0,0,0,0,1,0,0,0,0,0,0,0,'Sentinel Aynasha - Say Line 2'),
(1171100,9,3,0,0,0,100,0,4000,4000,0,0,1,3,0,0,0,0,0,1,0,0,0,0,0,0,0,'Sentinel Aynasha - Say Line 3'),
(1171100,9,4,0,0,0,100,0,3000,3000,0,0,1,4,0,0,0,0,0,1,0,0,0,0,0,0,0,'Sentinel Aynasha - Say Line 4'),
(1171100,9,5,0,0,0,100,0,4000,4000,0,0,28,18373,0,0,0,0,0,1,0,0,0,0,0,0,0,'Sentinel Aynasha - Remove Aura Aynasha''s Root'),
(1171100,9,6,0,0,0,100,0,1000,1000,0,0,53,1,810117110,0,0,0,0,1,0,0,0,0,0,0,0,'Sentinel Aynasha - Run Her Path Off (despawns at its end)');
