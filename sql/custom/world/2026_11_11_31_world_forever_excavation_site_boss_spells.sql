-- WoW Forever: Excavation Site: Wetlands (map 2998), the abilities of Saltspine and Shadetooth. Until now both fought in
-- melee only (no script). Which spells they cast is real live data:
-- Data from Efficient Games (https://efficient.games), licensed under CC BY 4.0 (EG Link output for 70862, creature spell
-- casts seen on live kills). Changed: we only took the spell ids; the timers below are INFERRED from the cast counts
-- (Saltspine: Tendon Rip 6 and Dust Storm 9 casts over 3 kills; Shadetooth: Rend 6, Terrifying Roar 6 and Enrage 3 over 3 kills).
-- Safe to run again.
UPDATE `creature_template` SET `AIName` = 'SmartAI', `ScriptName` = '' WHERE `entry` IN (260322,260325);
DELETE FROM `smart_scripts` WHERE `source_type` = 0 AND `entryorguid` IN (260322,260325);
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`Difficulties`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`event_param_string`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`action_param7`,`action_param_string`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_param4`,`target_param_string`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(260322,0,0,0,'',0,0,100,0,4000,7000,12000,16000,0,'',11,3604,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Saltspine - Tendon Rip on victim (spell from EG Link, timer INFERRED)'),
(260322,0,1,0,'',0,0,100,0,6000,9000,9000,13000,0,'',11,1316382,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Saltspine - Dust Storm (spell from EG Link, timer INFERRED)'),
(260325,0,0,0,'',0,0,100,0,3000,6000,12000,16000,0,'',11,13445,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'Shadetooth - Rend on victim (spell from EG Link, timer INFERRED)'),
(260325,0,1,0,'',0,0,100,0,8000,12000,14000,18000,0,'',11,14100,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Shadetooth - Terrifying Roar (spell from EG Link, timer INFERRED)'),
(260325,0,2,0,'',2,0,100,1,0,30,0,0,0,'',11,1284435,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Shadetooth - Enrage at 30% health (spell from EG Link, health INFERRED)');
