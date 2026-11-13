-- Scarlet Monastery: High Inquisitor Whitemane comes in, fights, and raises Commander Mograine (fix found by SIN).
-- When Mograine first falls the instance script opens her door and sends her to point 100 with MovePoint (classic_instance_
-- scarlet_monastery). Her imported SmartAI waited for movement type 9, but MovePoint reports type 8 (POINT_MOTION_TYPE), so she
-- never reacted; the old arrival rows also left her immune to players, and her half-health rows stopped her attacks and chase
-- without sending her to Mograine. Now:
--   reset: phase 1, keep 1 health until the resurrection, immune to players, melee and chase on;
--   point 100: attackable, phase 2, attacks the party;
--   below 50%: Deep Sleep, phase 3, walks to the living Mograine (point 101, retried every 12 s if interrupted);
--   point 101: phase 4, casts Scarlet Resurrection every 5 s until it hits; then phase 5, the 1 health floor goes and she fights.
-- Her spells (ids 12-15) run in phases 2 and 5. Mograine's own rows are unchanged. Safe to run again.
DELETE FROM `smart_scripts` WHERE `entryorguid` = 3977 AND `source_type` = 0;
INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`, `Difficulties`, `event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `event_param_string`, `action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`, `action_param7`, `action_param_string`, `target_type`, `target_param1`, `target_param2`, `target_param3`, `target_param4`, `target_param_string`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
(3977,0,0,1,'',25,0,100,0,0,0,0,0,0,'',22,1,0,0,0,0,0,0,'',1,0,0,0,0,'',0.0,0.0,0.0,0.0,'Cathedral - Reset - wait behind the door'),
(3977,0,1,2,'',61,0,100,0,0,0,0,0,0,'',42,1,0,0,0,0,0,0,'',1,0,0,0,0,'',0.0,0.0,0.0,0.0,'Cathedral - Reset - keep one health until resurrection'),
(3977,0,2,16,'',61,0,100,0,0,0,0,0,0,'',144,1,0,0,0,0,0,0,'',1,0,0,0,0,'',0.0,0.0,0.0,0.0,'Cathedral - Reset - restore player immunity'),
(3977,0,3,4,'',2,2,100,1,0,50,0,0,0,'',11,9256,3,0,0,0,0,0,'',1,0,0,0,0,'',0.0,0.0,0.0,0.0,'Cathedral - At half health in the first fight - Deep Sleep'),
(3977,0,4,5,'',61,0,100,0,0,0,0,0,0,'',20,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0.0,0.0,0.0,0.0,'VMaNGOS EventAI 397702'),
(3977,0,5,19,'',61,0,100,0,0,0,0,0,0,'',21,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0.0,0.0,0.0,0.0,'Cathedral - Sleep - suspend chase and approach the commander'),
(3977,0,6,7,'',34,1,100,1,8,100,0,0,0,'',144,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0.0,0.0,0.0,0.0,'Cathedral - Arrived at point 100 - become attackable'),
(3977,0,7,18,'',61,0,100,0,0,0,0,0,0,'',22,2,0,0,0,0,0,0,'',1,0,0,0,0,'',0.0,0.0,0.0,0.0,'Cathedral - Arrival - enter first combat phase'),
(3977,0,8,0,'',34,4,100,1,8,101,0,0,0,'',22,4,0,0,0,0,0,0,'',1,0,0,0,0,'',0.0,0.0,0.0,0.0,'Cathedral - Arrived at point 101 - prepare resurrection'),
(3977,0,9,0,'',0,8,100,0,500,500,5000,5000,0,'',11,9232,0,0,0,0,0,0,'',1,0,0,0,0,'',0.0,0.0,0.0,0.0,'Cathedral - Cast Scarlet Resurrection near Mograine until it hits'),
(3977,0,10,0,'',7,0,100,1,0,0,0,0,0,'',34,1,0,0,0,0,0,0,'',1,0,0,0,0,'',0.0,0.0,0.0,0.0,'VMaNGOS EventAI 397705'),
(3977,0,11,0,'',5,0,100,1,0,0,0,0,0,'',1,3,0,1,0,0,0,0,'',7,0,0,0,0,'',0.0,0.0,0.0,0.0,'VMaNGOS EventAI 397706'),
(3977,0,12,0,'',0,18,100,0,15000,30000,60000,60000,0,'',11,14515,0,0,0,0,0,0,'',6,0,0,0,0,'',0.0,0.0,0.0,0.0,'VMaNGOS spell list 39770'),
(3977,0,13,0,'',74,18,100,0,0,60,30000,60000,0,'',11,12039,0,0,0,0,0,0,'',9,0,0,30,0,'',0.0,0.0,0.0,0.0,'VMaNGOS spell list 39770'),
(3977,0,14,0,'',16,18,100,0,22187,30,10000,30000,0,'',11,22187,0,0,0,0,0,0,'',7,0,0,0,0,'',0.0,0.0,0.0,0.0,'VMaNGOS spell list 39770'),
(3977,0,15,0,'',0,18,100,0,0,2000,2000,3000,0,'',11,9481,0,0,0,0,0,0,'',2,0,0,0,0,'',0.0,0.0,0.0,0.0,'VMaNGOS spell list 39770'),
(3977,0,16,17,'',61,0,100,0,0,0,0,0,0,'',20,1,0,0,0,0,0,0,'',1,0,0,0,0,'',0.0,0.0,0.0,0.0,'Cathedral - Reset - restore melee'),
(3977,0,17,0,'',61,0,100,0,0,0,0,0,0,'',21,1,0,0,0,0,0,0,'',1,0,0,0,0,'',0.0,0.0,0.0,0.0,'Cathedral - Reset - restore chase'),
(3977,0,18,0,'',61,0,100,0,0,0,0,0,0,'',38,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0.0,0.0,0.0,0.0,'Cathedral - Arrival - engage the party'),
(3977,0,19,20,'',61,0,100,0,0,0,0,0,0,'',22,3,0,0,0,0,0,0,'',1,0,0,0,0,'',0.0,0.0,0.0,0.0,'Cathedral - Sleep - enter approach phase'),
(3977,0,20,0,'',61,0,100,0,0,0,0,0,0,'',69,101,0,0,1,0,0,0,'',19,3976,150,0,0,'',0.0,0.0,0.0,0.0,'Cathedral - Move to the living commander at point 101'),
(3977,0,21,22,'',31,8,100,0,9232,0,0,0,0,'',22,5,0,0,0,0,0,0,'',1,0,0,0,0,'',0.0,0.0,0.0,0.0,'Cathedral - Scarlet Resurrection hit - enter the final combat phase'),
(3977,0,22,23,'',61,0,100,0,0,0,0,0,0,'',42,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0.0,0.0,0.0,0.0,'Cathedral - Resurrection - clear one-health floor'),
(3977,0,23,24,'',61,0,100,0,0,0,0,0,0,'',20,1,0,0,0,0,0,0,'',1,0,0,0,0,'',0.0,0.0,0.0,0.0,'Cathedral - Resurrection - enable melee'),
(3977,0,24,0,'',61,0,100,0,0,0,0,0,0,'',21,1,0,0,0,0,0,0,'',1,0,0,0,0,'',0.0,0.0,0.0,0.0,'Cathedral - Resurrection - enable chase'),
(3977,0,25,0,'',0,4,100,0,12000,12000,12000,12000,0,'',69,101,0,0,1,0,0,0,'',19,3976,150,0,0,'',0.0,0.0,0.0,0.0,'Cathedral - Retry interrupted movement to point 101');
