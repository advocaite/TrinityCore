-- Classic 1.60: seven spawned vanilla creatures that have VMaNGOS 1.12 AI (EventAI and creature_spells) but had no AI here: no
-- smart_scripts rows, no C++ script, AIName empty, so they only melee. The world baseline replaced the whole smart_scripts table
-- and these creatures were not in it. Converted the way 2026_09_28_15 / 2026_10_11_24 convert VMaNGOS AI (spell list timers in
-- ms, cast flags as action_param2, cast target 0 -> self, 1 -> victim, 4 -> random hostile, 17 -> injured friend within 30 yd;
-- EventAI flee at 15% -> SMART_ACTION_FLEE_FOR_ASSIST, not repeatable):
--   Moonrage Watcher 1892, Moonrage Sentry 1893, Moonrage Elder 1896, Moonrage Armorer 3529, Moonrage Tailor 3531 (Silverpine),
--   Southsea Kidnapper 15685 (Barrens), Wickerman Guardian 15195 (Hallow's End).
-- Every spell exists in 70532. Safe to run again.

UPDATE `creature_template` SET `AIName` = 'SmartAI' WHERE `entry` IN (1892,1893,1896,3529,3531,15685,15195) AND `ScriptName` = '';
DELETE FROM `smart_scripts` WHERE `source_type` = 0 AND `entryorguid` IN (1892,1893,1896,3529,3531,15685,15195);
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`Difficulties`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`event_param_string`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`action_param7`,`action_param_string`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_param4`,`target_param_string`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(1892,0,0,0,'',2,0,100,1,0,15,0,0,0,'',25,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 189201'),
(1893,0,0,0,'',2,0,100,1,0,15,0,0,0,'',25,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 189301'),
(1893,0,1,0,'',0,0,100,0,1000,6000,12000,25000,0,'',11,6507,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'VMaNGOS spell list 18930'),
(1893,0,2,0,'',0,0,100,0,2000,20000,33000,50000,0,'',11,13730,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS spell list 18930'),
(1896,0,0,0,'',2,0,100,1,0,15,0,0,0,'',25,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 189601'),
(1896,0,1,0,'',0,0,100,0,0,10000,1000,10000,0,'',11,594,32,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS spell list 18960'),
(1896,0,2,0,'',74,0,100,0,0,60,15000,15000,0,'',11,2053,0,0,0,0,0,0,'',9,0,0,30,0,'',0,0,0,0,'VMaNGOS spell list 18960'),
(3529,0,0,0,'',2,0,100,1,0,15,0,0,0,'',25,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 352901'),
(3531,0,0,0,'',9,0,100,1,0,5,0,0,0,'',11,15657,1,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 353101'),
(3531,0,1,0,'',2,0,100,1,0,15,0,0,0,'',25,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 353102'),
(3531,0,2,0,'',0,0,100,0,1000,12000,24000,28000,0,'',11,7140,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS spell list 35310'),
(15685,0,0,0,'',2,0,100,1,0,15,0,0,0,'',25,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 1568501'),
(15685,0,1,0,'',0,0,100,0,6000,15000,7000,15000,0,'',11,6713,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS spell list 156850'),
(15195,0,0,0,'',9,0,100,0,0,5,8000,12000,0,'',11,18368,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 1519501'),
(15195,0,1,0,'',6,0,100,1,0,0,0,0,0,'',11,25007,3,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 1519503'),
(15195,0,2,0,'',0,0,100,0,8000,14000,9000,15000,0,'',11,19128,0,0,0,0,0,0,'',5,0,0,0,0,'',0,0,0,0,'VMaNGOS spell list 151950');
