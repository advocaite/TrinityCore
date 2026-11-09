-- Ruins of Lordaeron: The Abandoned 250631 comes out of the statue flying and not selectable, and only lands (gravity, selectable,
-- engage) when it reaches the courtyard point. It is aggressive while flying, so a player nearby pulls it on the way: the fight
-- replaces the flight, the "reached the courtyard" event never comes, and it stays flying and not selectable while it kills the
-- group (player report 2026-11-05). Now it is passive while flying, and it lands on the first hit or after 10 s at the latest.
DELETE FROM `smart_scripts` WHERE `source_type` = 0 AND `entryorguid` = 250631 AND `id` BETWEEN 50 AND 69;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`Difficulties`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`event_param_string`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`action_param7`,`action_param_string`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_param4`,`target_param_string`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(250631,0,50,51,'',54,0,100,0,0,0,0,0,0,'',60,1,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'The Abandoned - Just Summoned - Fly'),
(250631,0,51,52,'',61,0,100,0,0,0,0,0,0,'',8,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'The Abandoned - Just Summoned - Passive while flying'),
(250631,0,52,0,'',61,0,100,0,0,0,0,0,0,'',69,1,0,1,0,0,0,0,'',8,0,0,0,0,'',1599.2,359.91,62.52,0,'The Abandoned - Just Summoned - Fly out of the statue to the courtyard'),
(250631,0,53,54,'',34,0,100,0,8,1,0,0,0,'',60,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'The Abandoned - Reached the courtyard - Land'),
(250631,0,54,55,'',61,0,100,0,0,0,0,0,0,'',146,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'The Abandoned - Reached the courtyard - Selectable'),
(250631,0,55,56,'',61,0,100,0,0,0,0,0,0,'',8,2,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'The Abandoned - Reached the courtyard - Aggressive'),
(250631,0,56,0,'',61,0,100,0,0,0,0,0,0,'',38,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'The Abandoned - Reached the courtyard - Engage the instance'),
(250631,0,57,58,'',4,0,100,0,0,0,0,0,0,'',60,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'The Abandoned - Pulled before the courtyard - Land'),
(250631,0,58,59,'',61,0,100,0,0,0,0,0,0,'',146,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'The Abandoned - Pulled before the courtyard - Selectable'),
(250631,0,59,0,'',61,0,100,0,0,0,0,0,0,'',8,2,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'The Abandoned - Pulled before the courtyard - Aggressive'),
(250631,0,60,61,'',60,0,100,1,10000,10000,0,0,0,'',60,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'The Abandoned - 10 s after rising - Land (if the flight was cut short)'),
(250631,0,61,62,'',61,0,100,0,0,0,0,0,0,'',146,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'The Abandoned - 10 s after rising - Selectable'),
(250631,0,62,63,'',61,0,100,0,0,0,0,0,0,'',8,2,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'The Abandoned - 10 s after rising - Aggressive'),
(250631,0,63,0,'',61,0,100,0,0,0,0,0,0,'',38,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'The Abandoned - 10 s after rising - Engage the instance');
