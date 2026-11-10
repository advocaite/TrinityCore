-- Quest credits that nothing gave.
-- 5621/5624/5625/5648/5650 Garments of ...: the heal and Fortitude reactions were ported from VMaNGOS EventAI,
-- but its "complete quest" step (command 83, in the Fortitude-after-heal script) was dropped. Give the kill credit
-- the objective asks for ("Heal and fortify <guard>") to the priest who cast Fortitude, after the heal phase.
DELETE FROM `smart_scripts` WHERE `entryorguid` IN (12423,12427,12428,12429,12430) AND `source_type`=0 AND `id`=100;
UPDATE `smart_scripts` SET `link`=100 WHERE `entryorguid`=12423 AND `source_type`=0 AND `id` IN (6,10,13,16,19,22) AND `link`=0;
UPDATE `smart_scripts` SET `link`=100 WHERE `entryorguid`=12427 AND `source_type`=0 AND `id` IN (6,10,13,16,19,22) AND `link`=0;
UPDATE `smart_scripts` SET `link`=100 WHERE `entryorguid`=12428 AND `source_type`=0 AND `id` IN (6,10,13,16,19,22) AND `link`=0;
UPDATE `smart_scripts` SET `link`=100 WHERE `entryorguid`=12429 AND `source_type`=0 AND `id` IN (6,10,13,16,19,22) AND `link`=0;
UPDATE `smart_scripts` SET `link`=100 WHERE `entryorguid`=12430 AND `source_type`=0 AND `id` IN (6,10,13,16,19,22) AND `link`=0;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(12423,0,100,0,61,0,100,0,0,0,0,0,33,12423,0,0,0,0,0,7,0,0,0,0,0,0,0,'On Link (Fortitude after heal) - Quest Credit Heal and Fortify'),
(12427,0,100,0,61,0,100,0,0,0,0,0,33,12427,0,0,0,0,0,7,0,0,0,0,0,0,0,'On Link (Fortitude after heal) - Quest Credit Heal and Fortify'),
(12428,0,100,0,61,0,100,0,0,0,0,0,33,12428,0,0,0,0,0,7,0,0,0,0,0,0,0,'On Link (Fortitude after heal) - Quest Credit Heal and Fortify'),
(12429,0,100,0,61,0,100,0,0,0,0,0,33,12429,0,0,0,0,0,7,0,0,0,0,0,0,0,'On Link (Fortitude after heal) - Quest Credit Heal and Fortify'),
(12430,0,100,0,61,0,100,0,0,0,0,0,33,12430,0,0,0,0,0,7,0,0,0,0,0,0,0,'On Link (Fortitude after heal) - Quest Credit Heal and Fortify');

-- 8746/8762 Metzen the Reindeer: the gossip script cast retail spell 102760 "Quest Credit", which is not in the 70532 client
-- or serverside_spell, so the rescue never counted. Give the kill credit directly.
UPDATE `smart_scripts` SET `action_type`=33,`action_param1`=15664,`action_param2`=0,`target_type`=7,`comment`='Metzen the Reindeer - On Script - Quest Credit Metzen the Reindeer' WHERE `entryorguid`=1566400 AND `source_type`=9 AND `id`=1;

-- 93739 Exploring the Horde / 93963 Exploring the Alliance (Skyborne): "Obtain/Receive Instructions from Nazgrel / Randal Emerson"
-- are kill-type objectives on friendly NPCs and nothing gave them. Credit on talking to them (the "Speak with" parts already work).
-- Lady Sylvanas (same quest) has a C++ script, credited there.
UPDATE `creature_template` SET `AIName`='SmartAI' WHERE `entry`=275491 AND `AIName`='' AND `ScriptName`='';
DELETE FROM `smart_scripts` WHERE `entryorguid` IN (3230,275491) AND `source_type`=0 AND `id`=50;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(3230,0,50,0,64,0,100,0,0,0,0,0,33,3230,0,0,0,0,0,7,0,0,0,0,0,0,0,'Nazgrel - On Gossip Hello - Quest Credit Obtain Instructions (93739)'),
(275491,0,50,0,64,0,100,0,0,0,0,0,33,275491,0,0,0,0,0,7,0,0,0,0,0,0,0,'Randal Emerson - On Gossip Hello - Quest Credit Receive Instructions (93963)');

-- 99158 Dawn in the Mountains: the turn-in NPC Father Gavin had no quest-giver flag, so the quest could not be handed in.
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry`=1253;
