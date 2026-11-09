-- Bitter Rivals (310, Kharanos): the Unguarded Thunder Ale Barrel never appeared, so the quest could not be handed in.
-- In the Thunderbrew Distillery basement the Guarded Thunder Ale Barrel (269, spawn 30001037) and the Unguarded Thunder Ale Barrel
-- (270, spawn 30035875, not spawned by default) stand on the same spot. Handing Jarven Thunderbrew a Thunder Ale (Distracting Jarven,
-- 308) sends him away (action list 950000008, ported from VMaNGOS quest_end_scripts 308); the port moved him but did not swap the
-- barrels. Now the guarded barrel goes away for two minutes and the unguarded one, where Bitter Rivals is handed in, comes for two
-- minutes. Safe to run again.
DELETE FROM `smart_scripts` WHERE `entryorguid`=1373 AND `source_type`=0 AND `id` IN (1,2);
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`Difficulties`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`event_param_string`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`action_param7`,`action_param_string`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_param4`,`target_param_string`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(1373,0,1,0,'',20,0,100,0,308,0,0,0,0,'',41,0,120,0,0,0,0,0,'',14,30001037,269,0,0,'',0,0,0,0,'Jarven Thunderbrew - On Quest Distracting Jarven Rewarded - Despawn Guarded Thunder Ale Barrel for 120 s'),
(1373,0,2,0,'',20,0,100,0,308,0,0,0,0,'',70,120,0,0,0,0,0,0,'',14,30035875,270,0,0,'',0,0,0,0,'Jarven Thunderbrew - On Quest Distracting Jarven Rewarded - Show Unguarded Thunder Ale Barrel for 120 s');
