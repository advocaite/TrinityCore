-- 1324 The Missing Diplomat (Dustwallow, Private Hendel): the fight started but nothing ended it. VMaNGOS ended the map event as
-- a success when Hendel dropped to 30%: he gave up (faction friendly, immune to players, combat stop, stunned), his two Theramore
-- Sentries shouted and ran off, Lady Jaina Proudmoore, Archmage Tervosh and Pained teleported in (140 s), Tervosh encaged Hendel
-- and sent him to the tower, and the quest completed for players within 100 yards at 21 s. If Hendel evaded first (player dead
-- or gone) the event failed: the quest failed and Hendel and the sentries were reset.
-- Phases on Hendel: 1 idle, 2 fight (set on accept), 3 beaten. The reset-to-phase-1 event now only runs when idle, as for
-- Calvin Montague (SmartAI resets just before Reached Home, so it wiped the fight phase).
-- Not carried over: Jaina's and Tervosh's walk-in moves (SAI cannot move another creature; they stay at their arrival spots).

DELETE FROM `creature_text` WHERE `CreatureID`=4966 AND `GroupID` IN (0,1);
DELETE FROM `creature_text` WHERE `CreatureID`=5184 AND `GroupID` IN (0,1);
DELETE FROM `creature_text` WHERE `CreatureID`=4967 AND `GroupID` IN (0,1,2,3);
INSERT INTO `creature_text` (`CreatureID`,`GroupID`,`ID`,`Text`,`Type`,`Language`,`Probability`,`Emote`,`Duration`,`Sound`,`SoundPlayType`,`BroadcastTextId`,`TextRange`,`comment`) VALUES
(4966,0,0,'%s, too injured, gives up the chase.',16,0,100,0,0,0,0,8224,0,'Private Hendel - beaten (VMaNGOS)'),
(4966,1,0,'Please... please... Miss Proudmore. I didn''t mean to...',12,0,100,0,0,0,0,1759,0,'Private Hendel - encaged (VMaNGOS)'),
(5184,0,0,'Run!',12,0,34,0,0,0,0,7324,0,'Theramore Sentry - Hendel beaten (VMaNGOS)'),
(5184,0,1,'Don''t let them catch you!',12,0,33,0,0,0,0,7341,0,'Theramore Sentry - Hendel beaten (VMaNGOS)'),
(5184,0,2,'She''ll have our heads if we''re caught!',12,0,33,0,0,0,0,7342,0,'Theramore Sentry - Hendel beaten (VMaNGOS)'),
(5184,1,0,'It''s Proudmoore! Get out of here!',12,0,50,0,0,0,0,7325,0,'Theramore Sentry - Hendel beaten (VMaNGOS)'),
(5184,1,1,'Get out of here!',12,0,50,0,0,0,0,7340,0,'Theramore Sentry - Hendel beaten (VMaNGOS)'),
(4967,0,0,'Why don''t we deal with you now, Hendel? Lady Proudmoore will speak to you back in the tower.',12,0,100,0,0,0,0,1663,0,'Archmage Tervosh - The Missing Diplomat (VMaNGOS)'),
(4967,1,0,'I apologize for taking so long to get here. I wanted Lady Proudmoore to be present also.',12,0,100,1,0,0,0,1761,0,'Archmage Tervosh - The Missing Diplomat (VMaNGOS)'),
(4967,2,0,'We can only stay a few moments before returning to the tower. If you wish to speak to us more you may find us there.',12,0,100,1,0,0,0,1808,0,'Archmage Tervosh - The Missing Diplomat (VMaNGOS)'),
(4967,3,0,'Farewell. We shall speak again, I''m sure.',12,0,100,2,0,0,0,1772,0,'Archmage Tervosh - The Missing Diplomat (VMaNGOS)');

-- Hendel
UPDATE `smart_scripts` SET `event_phase_mask`=1 WHERE `entryorguid`=4966 AND `source_type`=0 AND `id`=0 AND `event_type`=25;
UPDATE `smart_scripts` SET `event_phase_mask`=2, `action_type`=80, `action_param1`=496610, `action_param2`=2, `comment`='Private Hendel - On Evade in the Fight - Event Failed (VMaNGOS EventAI 496601)'
WHERE `entryorguid`=4966 AND `source_type`=0 AND `id`=1 AND `event_type`=7;
UPDATE `smart_scripts` SET `event_phase_mask`=2, `action_type`=80, `action_param1`=496600, `action_param2`=2, `comment`='Private Hendel - At 30% in the Fight - Event Success (VMaNGOS EventAI 496602)'
WHERE `entryorguid`=4966 AND `source_type`=0 AND `id`=2 AND `event_type`=2;

DELETE FROM `smart_scripts` WHERE `entryorguid`=950000124 AND `source_type`=9 AND `id`=7;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(950000124,9,7,0,0,0,100,0,0,0,0,0,64,1,0,0,0,0,0,7,0,0,0,0,0,0,0,'Private Hendel - Store Quest Player');

DELETE FROM `smart_scripts` WHERE `entryorguid` IN (496600,496610) AND `source_type`=9;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(496600,9,0,0,0,0,100,0,0,0,0,0,22,3,0,0,0,0,0,1,0,0,0,0,0,0,0,'Private Hendel - Set Phase 3 (beaten)'),
(496600,9,1,0,0,0,100,0,0,0,0,0,2,35,0,0,0,0,0,1,0,0,0,0,0,0,0,'Private Hendel - Set Faction Friendly'),
(496600,9,2,0,0,0,100,0,0,0,0,0,144,1,0,0,0,0,0,1,0,0,0,0,0,0,0,'Private Hendel - Set Immune To PC On'),
(496600,9,3,0,0,0,100,0,0,0,0,0,27,0,0,0,0,0,0,1,0,0,0,0,0,0,0,'Private Hendel - Combat Stop'),
(496600,9,4,0,0,0,100,0,0,0,0,0,45,1,1,0,0,0,0,10,20303721,5184,0,0,0,0,0,'Private Hendel - Theramore Sentry 1 Runs'),
(496600,9,5,0,0,0,100,0,0,0,0,0,45,1,2,0,0,0,0,10,20303722,5184,0,0,0,0,0,'Private Hendel - Theramore Sentry 2 Runs'),
(496600,9,6,0,0,0,100,0,0,0,0,0,12,4968,3,140000,0,0,0,8,0,0,0,-2875.03,-3349.14,35.9534,1.8326,'Private Hendel - Summon Lady Jaina Proudmoore'),
(496600,9,7,0,0,0,100,0,0,0,0,0,12,4967,3,140000,0,0,0,8,0,0,0,-2876.28,-3347.27,35.7761,2.84489,'Private Hendel - Summon Archmage Tervosh'),
(496600,9,8,0,0,0,100,0,0,0,0,0,12,4965,3,140000,0,0,0,8,0,0,0,-2873.21,-3346.3,35.8944,3.7001,'Private Hendel - Summon Pained'),
(496600,9,9,0,0,0,100,0,1000,1000,0,0,1,0,0,0,0,0,0,1,0,0,0,0,0,0,0,'Private Hendel - Emote Line 0 (gives up)'),
(496600,9,10,0,0,0,100,0,0,0,0,0,17,64,0,0,0,0,0,1,0,0,0,0,0,0,0,'Private Hendel - Emote State Stun'),
(496600,9,11,0,0,0,100,0,17000,17000,0,0,1,1,0,0,0,0,0,1,0,0,0,0,0,0,0,'Private Hendel - Say Line 1'),
(496600,9,12,0,0,0,100,0,3000,3000,0,0,152,1324,0,0,0,0,0,18,100,0,0,0,0,0,0,'Private Hendel - Complete Quest The Missing Diplomat (100 yards)'),
(496600,9,13,0,0,0,100,0,0,0,0,0,41,0,0,0,0,0,0,1,0,0,0,0,0,0,0,'Private Hendel - Despawn (sent to the tower)'),
(496610,9,0,0,0,0,100,0,0,0,0,0,6,1324,0,0,0,0,0,12,1,0,0,0,0,0,0,'Private Hendel - Fail Quest The Missing Diplomat (VMaNGOS generic_scripts 132400)'),
(496610,9,1,0,0,0,100,0,0,0,0,0,2,0,0,0,0,0,0,10,20303721,5184,0,0,0,0,0,'Private Hendel - Theramore Sentry 1 Restore Faction'),
(496610,9,2,0,0,0,100,0,0,0,0,0,2,0,0,0,0,0,0,10,20303722,5184,0,0,0,0,0,'Private Hendel - Theramore Sentry 2 Restore Faction'),
(496610,9,3,0,0,0,100,0,0,0,0,0,2,0,0,0,0,0,0,1,0,0,0,0,0,0,0,'Private Hendel - Restore Faction'),
(496610,9,4,0,0,0,100,0,0,0,0,0,42,0,0,0,0,0,0,1,0,0,0,0,0,0,0,'Private Hendel - Remove Invincibility'),
(496610,9,5,0,0,0,100,0,0,0,0,0,82,2,0,0,0,0,0,1,0,0,0,0,0,0,0,'Private Hendel - Add Quest-Giver Flag'),
(496610,9,6,0,0,0,100,0,0,0,0,0,22,1,0,0,0,0,0,1,0,0,0,0,0,0,0,'Private Hendel - Set Phase 1');

-- Theramore Sentries: on Hendel's word they stop fighting, shout and run to the VMaNGOS points (point 1324); their existing
-- movement-inform event despawns them there, and Hendel's respawn brings them back.
DELETE FROM `smart_scripts` WHERE `entryorguid`=5184 AND `source_type`=0 AND `id` BETWEEN 2 AND 11;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(5184,0,2,3,38,0,100,0,1,1,0,0,2,0,0,0,0,0,0,1,0,0,0,0,0,0,0,'Theramore Sentry - On Data Set 1 1 (Hendel beaten) - Restore Faction'),
(5184,0,3,4,61,0,100,0,0,0,0,0,27,0,0,0,0,0,0,1,0,0,0,0,0,0,0,'Theramore Sentry - On Link - Combat Stop'),
(5184,0,4,5,61,0,100,0,0,0,0,0,1,0,0,0,0,0,0,1,0,0,0,0,0,0,0,'Theramore Sentry - On Link - Say Line 0'),
(5184,0,5,6,61,0,100,0,0,0,0,0,59,1,0,0,0,0,0,1,0,0,0,0,0,0,0,'Theramore Sentry - On Link - Set Run'),
(5184,0,6,0,61,0,100,0,0,0,0,0,69,1324,0,0,0,0,0,8,0,0,0,-2916.96,-3352.19,29.3314,0,'Theramore Sentry - On Link - Run Off (point 1324)'),
(5184,0,7,8,38,0,100,0,1,2,0,0,2,0,0,0,0,0,0,1,0,0,0,0,0,0,0,'Theramore Sentry - On Data Set 1 2 (Hendel beaten) - Restore Faction'),
(5184,0,8,9,61,0,100,0,0,0,0,0,27,0,0,0,0,0,0,1,0,0,0,0,0,0,0,'Theramore Sentry - On Link - Combat Stop'),
(5184,0,9,10,61,0,100,0,0,0,0,0,1,1,0,0,0,0,0,1,0,0,0,0,0,0,0,'Theramore Sentry - On Link - Say Line 1'),
(5184,0,10,11,61,0,100,0,0,0,0,0,59,1,0,0,0,0,0,1,0,0,0,0,0,0,0,'Theramore Sentry - On Link - Set Run'),
(5184,0,11,0,61,0,100,0,0,0,0,0,69,1324,0,0,0,0,0,8,0,0,0,-2916.89,-3333.04,30.229,0,'Theramore Sentry - On Link - Run Off (point 1324)');

-- Pained carries the visit's timeline (she lives the full 140 s): Tervosh's spells on Hendel and his lines, Jaina's arrival.
UPDATE `creature_template` SET `AIName`='SmartAI' WHERE `entry`=4965 AND `AIName`='' AND `ScriptName`='';
DELETE FROM `smart_scripts` WHERE `entryorguid`=4965 AND `source_type`=0 AND `id`=0;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(4965,0,0,0,54,0,100,0,0,0,0,0,80,496500,2,0,0,0,0,1,0,0,0,0,0,0,0,'Pained - Just Summoned - The Missing Diplomat Visit');
DELETE FROM `smart_scripts` WHERE `entryorguid`=496500 AND `source_type`=9;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(496500,9,0,0,0,0,100,0,0,0,0,0,86,7143,2,19,4968,20,0,19,4968,20,0,0,0,0,0,'Pained - Lady Jaina Proudmoore Casts Simple Teleport Group (arrival)'),
(496500,9,1,0,0,0,100,0,4000,4000,0,0,69,0,0,0,0,0,0,8,0,0,0,-2882.18,-3346.15,33.9913,3.27056,'Pained - Move to Hendel'),
(496500,9,2,0,0,0,100,0,2000,2000,0,0,86,7081,2,19,4967,30,0,19,4966,40,0,0,0,0,0,'Pained - Archmage Tervosh Casts Encage on Private Hendel'),
(496500,9,3,0,0,0,100,0,5000,5000,0,0,1,0,0,0,0,0,0,19,4967,30,0,0,0,0,0,'Pained - Archmage Tervosh Says Line 0'),
(496500,9,4,0,0,0,100,0,8000,8000,0,0,86,7079,2,19,4967,30,0,19,4966,40,0,0,0,0,0,'Pained - Archmage Tervosh Casts Simple Teleport Other on Private Hendel'),
(496500,9,5,0,0,0,100,0,7000,7000,0,0,1,1,0,0,0,0,0,19,4967,30,0,0,0,0,0,'Pained - Archmage Tervosh Says Line 1'),
(496500,9,6,0,0,0,100,0,8000,8000,0,0,1,2,0,0,0,0,0,19,4967,30,0,0,0,0,0,'Pained - Archmage Tervosh Says Line 2'),
(496500,9,7,0,0,0,100,0,90000,90000,0,0,1,3,0,0,0,0,0,19,4967,30,0,0,0,0,0,'Pained - Archmage Tervosh Says Line 3'),
(496500,9,8,0,0,0,100,0,10000,10000,0,0,86,7078,2,19,4968,20,0,19,4968,20,0,0,0,0,0,'Pained - Lady Jaina Proudmoore Casts Simple Teleport Group (leaving)');
