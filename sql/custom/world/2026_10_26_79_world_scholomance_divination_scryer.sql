-- Scholomance, the Divination Scryer: placing it (Place Scryer 23204, item 18746) at the Great Ossuary did nothing, so Death Knight
-- Darkreaver never appeared and Judgment and Redemption (7647), Again Into the Great Ossuary (7666, 7669) and The Darkreaver Menace
-- (7668, 8258) could not be finished. The ten waves are scripted in C++ (classic_go_divination_scryer, from VMaNGOS event_scripts
-- 8436 and generic_scripts 843600-843611). This file gives the spirits their VMaNGOS spells (creature_spells 145110-145210) and their
-- judgement reaction (creature_ai_events: hit by the matching paladin judgement, a spirit casts Holy Blast on its kin and emotes),
-- and gives Darkreaver his spawn yell. Malicious Spirit's second spell (12721) is not in the 70532 client and is left out.

UPDATE `gameobject_template` SET `ScriptName`='classic_go_divination_scryer', `AIName`='' WHERE `entry`=179708;
UPDATE `creature_template` SET `AIName`='SmartAI', `ScriptName`='' WHERE `entry` IN (14511,14512,14513,14514,14518,14519,14520,14521);

DELETE FROM `creature_text` WHERE `CreatureID` IN (14511,14512,14513,14514,14518,14519,14520,14521);
INSERT INTO `creature_text` (`CreatureID`,`GroupID`,`ID`,`Text`,`Type`,`Language`,`Probability`,`Emote`,`Duration`,`Sound`,`SoundPlayType`,`BroadcastTextId`,`TextRange`,`comment`) VALUES
(14511,0,0,'%s emerges from the darkness, drawn out by the divination scryer!',16,0,100,0,0,0,0,9781,0,'Shadowed Spirit - appears (VMaNGOS generic_scripts 843609)'),
(14512,0,0,'%s emerges from the darkness, drawn out by the divination scryer!',16,0,100,0,0,0,0,9781,0,'Corrupted Spirit - appears (VMaNGOS generic_scripts 843607)'),
(14513,0,0,'%s emerges from the darkness, drawn out by the divination scryer!',16,0,100,0,0,0,0,9781,0,'Malicious Spirit - appears (VMaNGOS generic_scripts 843603)'),
(14519,0,0,'%s emerges from the darkness, drawn out by the divination scryer!',16,0,100,0,0,0,0,9781,0,'Aspect of Corruption - appears (VMaNGOS generic_scripts 843608)'),
(14511,1,0,'%s recoils in pain as the Judgment of Light washes over it!',16,0,100,0,0,0,0,9773,0,'Shadowed Spirit - judged'),
(14521,1,0,'%s recoils in pain as the Judgment of Light washes over it!',16,0,100,0,0,0,0,9773,0,'Aspect of Shadow - judged'),
(14512,1,0,'%s recoils in pain as the Judgment of Righteousness washes over it!',16,0,100,0,0,0,0,9775,0,'Corrupted Spirit - judged'),
(14519,1,0,'%s recoils in pain as the Judgment of Righteousness washes over it!',16,0,100,0,0,0,0,9775,0,'Aspect of Corruption - judged'),
(14513,1,0,'%s recoils in pain as the Judgment of Justice washes over it!',16,0,100,0,0,0,0,9774,0,'Malicious Spirit - judged'),
(14520,1,0,'%s recoils in pain as the Judgment of Justice washes over it!',16,0,100,0,0,0,0,9774,0,'Aspect of Malice - judged'),
(14514,1,0,'%s recoils in pain as the Judgment of Wisdom washes over it!',16,0,100,0,0,0,0,9732,0,'Banal Spirit - judged'),
(14518,1,0,'%s recoils in pain as the Judgment of Wisdom washes over it!',16,0,100,0,0,0,0,9732,0,'Aspect of Banality - judged');

DELETE FROM `smart_scripts` WHERE `entryorguid` IN (14511,14512,14513,14514,14518,14519,14520,14521) AND `source_type`=0;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(14511,0,0,0,0,0,100,0,0,3000,0,21000,11,12739,0,0,0,0,0,2,0,0,0,0,0,0,0,'Shadowed Spirit - In Combat - Cast ''Shadow Bolt'' (VMaNGOS creature_spells)'),
(14511,0,1,0,0,0,100,0,2000,16000,21000,21000,11,22417,32,0,0,0,0,1,0,0,0,0,0,0,0,'Shadowed Spirit - In Combat - Cast ''Shadow Shield'' (VMaNGOS creature_spells)'),
(14511,0,2,0,0,0,100,0,10000,20000,23000,23000,11,15245,0,0,0,0,0,2,0,0,0,0,0,0,0,'Shadowed Spirit - In Combat - Cast ''Shadow Bolt Volley'' (VMaNGOS creature_spells)'),
(14511,0,3,0,0,0,100,0,11000,13000,18000,18000,11,22575,0,0,0,0,0,2,0,0,0,0,0,0,0,'Shadowed Spirit - In Combat - Cast ''Shadow Shock'' (VMaNGOS creature_spells)'),
(14511,0,4,5,8,0,100,0,20346,2,0,0,11,23269,0,0,0,0,0,1,0,0,0,0,0,0,0,'Shadowed Spirit - On Hit By ''Judgement of Light'' - Cast ''Holy Blast'' (VMaNGOS creature_ai_events)'),
(14511,0,5,0,61,0,100,0,0,0,0,0,1,1,0,0,0,0,0,1,0,0,0,0,0,0,0,'Shadowed Spirit - On Link - Emote Recoils in Pain'),
(14512,0,0,1,8,0,100,0,20286,2,0,0,11,23269,0,0,0,0,0,1,0,0,0,0,0,0,0,'Corrupted Spirit - On Hit By ''Judgement of Righteousness'' - Cast ''Holy Blast'' (VMaNGOS creature_ai_events)'),
(14512,0,1,0,61,0,100,0,0,0,0,0,1,1,0,0,0,0,0,1,0,0,0,0,0,0,0,'Corrupted Spirit - On Link - Emote Recoils in Pain'),
(14513,0,0,0,0,0,100,0,2000,15000,8000,18000,11,13738,32,0,0,0,0,2,0,0,0,0,0,0,0,'Malicious Spirit - In Combat - Cast ''Rend'' (VMaNGOS creature_spells)'),
(14513,0,1,2,8,0,100,0,20184,2,0,0,11,23269,0,0,0,0,0,1,0,0,0,0,0,0,0,'Malicious Spirit - On Hit By ''Judgement of Justice'' - Cast ''Holy Blast'' (VMaNGOS creature_ai_events)'),
(14513,0,2,0,61,0,100,0,0,0,0,0,1,1,0,0,0,0,0,1,0,0,0,0,0,0,0,'Malicious Spirit - On Link - Emote Recoils in Pain'),
(14514,0,0,0,0,0,100,0,0,23000,0,23000,11,23262,32,0,0,0,0,2,0,0,0,0,0,0,0,'Banal Spirit - In Combat - Cast ''Demoralize'' (VMaNGOS creature_spells)'),
(14514,0,1,2,8,0,100,0,20355,2,0,0,11,23269,0,0,0,0,0,1,0,0,0,0,0,0,0,'Banal Spirit - On Hit By ''Judgement of Wisdom'' - Cast ''Holy Blast'' (VMaNGOS creature_ai_events)'),
(14514,0,2,0,61,0,100,0,0,0,0,0,1,1,0,0,0,0,0,1,0,0,0,0,0,0,0,'Banal Spirit - On Link - Emote Recoils in Pain'),
(14518,0,0,0,0,0,100,0,13000,13000,29000,29000,11,16247,32,0,0,0,0,2,0,0,0,0,0,0,0,'Aspect of Banality - In Combat - Cast ''Curse of Thorns'' (VMaNGOS creature_spells)'),
(14518,0,1,0,0,0,100,0,5000,5000,15000,18000,11,8140,32,0,0,0,0,5,0,0,0,0,0,0,0,'Aspect of Banality - In Combat - Cast ''Befuddlement'' (VMaNGOS creature_spells)'),
(14518,0,2,0,0,0,100,0,11000,11000,11000,11000,11,16244,32,0,0,0,0,2,0,0,0,0,0,0,0,'Aspect of Banality - In Combat - Cast ''Demoralizing Shout'' (VMaNGOS creature_spells)'),
(14518,0,3,4,8,0,100,0,20355,2,0,0,11,23269,0,0,0,0,0,1,0,0,0,0,0,0,0,'Aspect of Banality - On Hit By ''Judgement of Wisdom'' - Cast ''Holy Blast'' (VMaNGOS creature_ai_events)'),
(14518,0,4,0,61,0,100,0,0,0,0,0,1,1,0,0,0,0,0,1,0,0,0,0,0,0,0,'Aspect of Banality - On Link - Emote Recoils in Pain'),
(14519,0,0,0,0,0,100,0,15000,15000,15000,15000,11,23268,32,0,0,0,0,2,0,0,0,0,0,0,0,'Aspect of Corruption - In Combat - Cast ''Shadow Word: Pain'' (VMaNGOS creature_spells)'),
(14519,0,1,0,0,0,100,0,15000,15000,15000,15000,11,13619,32,0,0,0,0,2,0,0,0,0,0,0,0,'Aspect of Corruption - In Combat - Cast ''Wracking Pains'' (VMaNGOS creature_spells)'),
(14519,0,2,3,8,0,100,0,20286,2,0,0,11,23269,0,0,0,0,0,1,0,0,0,0,0,0,0,'Aspect of Corruption - On Hit By ''Judgement of Righteousness'' - Cast ''Holy Blast'' (VMaNGOS creature_ai_events)'),
(14519,0,3,0,61,0,100,0,0,0,0,0,1,1,0,0,0,0,0,1,0,0,0,0,0,0,0,'Aspect of Corruption - On Link - Emote Recoils in Pain'),
(14520,0,0,0,0,0,100,0,15000,15000,15000,15000,11,13738,0,0,0,0,0,2,0,0,0,0,0,0,0,'Aspect of Malice - In Combat - Cast ''Rend'' (VMaNGOS creature_spells)'),
(14520,0,1,0,0,0,100,0,11000,11000,11000,11000,11,16098,32,0,0,0,0,2,0,0,0,0,0,0,0,'Aspect of Malice - In Combat - Cast ''Curse of Blood'' (VMaNGOS creature_spells)'),
(14520,0,2,3,8,0,100,0,20184,2,0,0,11,23269,0,0,0,0,0,1,0,0,0,0,0,0,0,'Aspect of Malice - On Hit By ''Judgement of Justice'' - Cast ''Holy Blast'' (VMaNGOS creature_ai_events)'),
(14520,0,3,0,61,0,100,0,0,0,0,0,1,1,0,0,0,0,0,1,0,0,0,0,0,0,0,'Aspect of Malice - On Link - Emote Recoils in Pain'),
(14521,0,0,0,0,0,100,0,21000,21000,21000,21000,11,17289,0,0,0,0,0,2,0,0,0,0,0,0,0,'Aspect of Shadow - In Combat - Cast ''Shadow Shock'' (VMaNGOS creature_spells)'),
(14521,0,1,0,0,0,100,0,0,3000,3000,9000,11,15472,0,0,0,0,0,2,0,0,0,0,0,0,0,'Aspect of Shadow - In Combat - Cast ''Shadow Bolt'' (VMaNGOS creature_spells)'),
(14521,0,2,0,0,0,100,0,13000,13000,10000,10000,11,17228,0,0,0,0,0,2,0,0,0,0,0,0,0,'Aspect of Shadow - In Combat - Cast ''Shadow Bolt Volley'' (VMaNGOS creature_spells)'),
(14521,0,3,0,0,0,100,0,12000,12000,12000,12000,11,22417,32,0,0,0,0,1,0,0,0,0,0,0,0,'Aspect of Shadow - In Combat - Cast ''Shadow Shield'' (VMaNGOS creature_spells)'),
(14521,0,4,5,8,0,100,0,20346,2,0,0,11,23269,0,0,0,0,0,1,0,0,0,0,0,0,0,'Aspect of Shadow - On Hit By ''Judgement of Light'' - Cast ''Holy Blast'' (VMaNGOS creature_ai_events)'),
(14521,0,5,0,61,0,100,0,0,0,0,0,1,1,0,0,0,0,0,1,0,0,0,0,0,0,0,'Aspect of Shadow - On Link - Emote Recoils in Pain');

-- Darkreaver yells when he appears (VMaNGOS creature_ai_events 1451602, broadcast 9784; the text row already exists).
DELETE FROM `smart_scripts` WHERE `entryorguid`=14516 AND `source_type`=0 AND `id`=5;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(14516,0,5,0,54,0,100,0,0,0,0,0,1,0,0,0,0,0,0,1,0,0,0,0,0,0,0,'Death Knight Darkreaver - On Just Summoned - Say Line 0 (VMaNGOS creature_ai_events 1451602)');
