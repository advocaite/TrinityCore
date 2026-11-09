-- Blizzard hotfix 6 November 2026: "Zaetar's Spirit now spawns after defeating Princess Theradras in Maraudon."
-- Princess Theradras (12201) runs the VMaNGOS SmartAI script. On death it made the dead boss cast 21896 on the killer,
-- and Zaetar's Spirit (12238) only spoke when hit by 21896. A dead caster casting is not reliable, so the spirit could be
-- missing, and with it the turn-in for "Legends of Maraudon". Now the death event summons the spirit straight away, at
-- the place the core script boss_princess_theradras uses (manual despawn, so it stays until the instance resets), and
-- the spirit speaks as soon as it is summoned.
-- Our world data also had the spirit standing there from the start (VMaNGOS spawn 20055352, same place). On live it only
-- shows up after the kill, so that spawn goes.
-- Safe to run again.
DELETE FROM `creature_addon` WHERE `guid` IN (SELECT `guid` FROM `creature` WHERE `id`=12238 AND `map`=349);
DELETE FROM `creature` WHERE `id`=12238 AND `map`=349;
DELETE FROM `smart_scripts` WHERE `source_type`=0 AND `entryorguid`=12201 AND `id`=0;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`Difficulties`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`event_param_string`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`action_param7`,`action_param_string`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_param4`,`target_param_string`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(12201,0,0,0,'',6,0,100,0,0,0,0,0,0,'',12,12238,8,0,0,0,0,0,'',8,0,0,0,0,'',28.1887,62.3964,-123.161,4.31096,'Princess Theradras - On Death - Summon Zaetar''s Spirit');

DELETE FROM `smart_scripts` WHERE `source_type`=0 AND `entryorguid`=12238 AND `id`=0;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`Difficulties`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`event_param_string`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`action_param7`,`action_param_string`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_param4`,`target_param_string`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(12238,0,0,0,'',54,0,100,0,0,0,0,0,0,'',1,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Zaetar''s Spirit - Just Summoned - Say Line 0');
