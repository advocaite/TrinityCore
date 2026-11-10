-- Covert Ops - Alpha (1079) / Beta (1080), Stonetalon: the NG-5 charges and remote detonators from the Covert Ops Pack
-- did nothing. Their spells send events 694/693 (set charge Blue/Red) and 692/691 (detonate Blue/Red), and nothing in
-- our world answered them, so no charge appeared and the detonator (needs the charge as its spell focus) could not be
-- used. Questie Forever corrections (Questie team, Questie/QuestieDB, master 4ab64311) mark these as the quests'
-- "Place the NG-5 Explosives" step at the wagons 19591 (Blue) and 19600 (Red), both spawned here.
-- Positions from the vanilla events 693/694 (vMaNGOS world, GPL-2). Detonating opens the explosives wagon next to the
-- charge and removes the charge. The plans and documents chests (19602/19603) were already lootable, so the quests
-- could be finished before; this makes the explosives step work as intended.
DELETE FROM `event_script_names` WHERE `Id` IN (691,692,693,694);
INSERT INTO `event_script_names` (`Id`,`ScriptName`) VALUES (691,'SmartEventTrigger'),(692,'SmartEventTrigger'),(693,'SmartEventTrigger'),(694,'SmartEventTrigger');
DELETE FROM `smart_scripts` WHERE `entryorguid` IN (691,692,693,694) AND `source_type`=3;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_param4`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(694,3,0,0,87,0,100,0,0,0,0,0,0,50,19601,60,1,0,0,0,8,0,0,0,0,1168.44,51.1277,0.0603573,5.48173,'Set NG-5 Charge (Blue) - Summon NG-5 Explosives (Blue) for 60 s'),
(693,3,0,0,87,0,100,0,0,0,0,0,0,50,19592,60,1,0,0,0,8,0,0,0,0,1048.82,-442.209,4.7429,2.17129,'Set NG-5 Charge (Red) - Summon NG-5 Explosives (Red) for 60 s'),
(692,3,0,0,87,0,100,0,0,0,0,0,0,9,0,0,0,0,0,0,20,19547,80,0,0,0,0,0,0,'Remote Detonate Blue - Activate Venture Co. Explosives Wagon'),
(692,3,1,0,87,0,100,0,0,0,0,0,0,41,0,0,0,0,0,0,20,19601,80,0,0,0,0,0,0,'Remote Detonate Blue - Despawn NG-5 Explosives (Blue)'),
(691,3,0,0,87,0,100,0,0,0,0,0,0,9,0,0,0,0,0,0,20,20899,80,0,0,0,0,0,0,'Remote Detonate Red - Activate Venture Co. Explosives Wagon'),
(691,3,1,0,87,0,100,0,0,0,0,0,0,41,0,0,0,0,0,0,20,19592,80,0,0,0,0,0,0,'Remote Detonate Red - Despawn NG-5 Explosives (Red)');
