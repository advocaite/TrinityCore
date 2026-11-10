-- Quest summons that the quest owner could not attack (VMaNGOS behaviour restored).
-- 1474/1513/1739 The Binding: the summoned Succubus stayed friendly (faction 35), so warlocks could not fight it.
-- VMaNGOS: 4 s after the summon it turns hostile (faction 14) and attacks the summoner.
DELETE FROM `smart_scripts` WHERE `entryorguid`=5677 AND `source_type`=0 AND `id` IN (2,3,4);
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(5677,0,2,0,54,0,100,0,0,0,0,0,67,1,4000,4000,0,0,100,1,0,0,0,0,0,0,0,'Summoned Succubus - On Just Summoned - Create Timed Event 1 (4 s)'),
(5677,0,3,4,59,0,100,0,1,0,0,0,2,14,0,0,0,0,0,1,0,0,0,0,0,0,0,'Summoned Succubus - On Timed Event 1 - Set Faction Hostile'),
(5677,0,4,0,61,0,100,0,0,0,0,0,49,0,0,0,0,0,0,23,0,0,0,0,0,0,0,'Summoned Succubus - On Link - Attack Summoner');

-- 7603 Kroshius' Infernal Core: Fel Fire (event 8328) summons a new Kroshius, but the template keeps IMMUNE_TO_PC/NPC (768),
-- the peaceful faction 1194 and the lying-down stand state, so nobody could kill him. VMaNGOS event 8328 removes the flags,
-- sets faction 14 and stands him up. Only the summoned copy changes; the spawned corpse keeps its look.
DELETE FROM `smart_scripts` WHERE `entryorguid`=14467 AND `source_type`=0 AND `id` IN (2,3,4,5,6,7);
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(14467,0,2,0,54,0,100,0,0,0,0,0,67,1,1500,1500,0,0,100,1,0,0,0,0,0,0,0,'Kroshius - On Just Summoned - Create Timed Event 1'),
(14467,0,3,4,59,0,100,0,1,0,0,0,91,7,0,0,0,0,0,1,0,0,0,0,0,0,0,'Kroshius - On Timed Event 1 - Stand Up'),
(14467,0,4,5,61,0,100,0,0,0,0,0,144,0,0,0,0,0,0,1,0,0,0,0,0,0,0,'Kroshius - On Link - Remove Immune To PC'),
(14467,0,5,6,61,0,100,0,0,0,0,0,145,0,0,0,0,0,0,1,0,0,0,0,0,0,0,'Kroshius - On Link - Remove Immune To NPC'),
(14467,0,6,7,61,0,100,0,0,0,0,0,2,14,0,0,0,0,0,1,0,0,0,0,0,0,0,'Kroshius - On Link - Set Faction Hostile'),
(14467,0,7,0,61,0,100,0,0,0,0,0,49,0,0,0,0,0,0,23,0,0,0,0,0,0,0,'Kroshius - On Link - Attack Summoner');
