-- Zephras Isle: two gossip options did nothing (no action, no SmartAI, no script).

-- 1) The Missing Scholar (92849): "<Pick him up and carry him.>" on Fillion Flamebreeze (253002, gossip 40877) gave nothing, so
--    "Carry Fillion Flamebreeze to safety" (kill credit 253281) could never finish. The client carry spells (1258429 "Saving Fillion
--    Flamebreeze" = vehicle + phase aura on the player) are not used: the phase aura is untested here. Instead Fillion follows the
--    player and gives the credit when he reaches the waiting Fillion (253284) at the safe spot, then despawns and respawns.
UPDATE `creature_template` SET `AIName` = 'SmartAI' WHERE `entry` = 253002;
DELETE FROM `smart_scripts` WHERE `entryorguid` = 253002 AND `source_type` = 0;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`Difficulties`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`event_param_string`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`action_param7`,`action_param_string`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_param4`,`target_param_string`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(253002,0,0,1,'',62,0,100,0,40877,0,0,0,0,'',64,1,0,0,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'Fillion Flamebreeze - On Gossip Option 0 Selected - Store Invoker'),
(253002,0,1,2,'',61,0,100,0,0,0,0,0,0,'',72,0,0,0,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'Fillion Flamebreeze - Linked - Close Gossip'),
(253002,0,2,3,'',61,0,100,0,0,0,0,0,0,'',81,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Fillion Flamebreeze - Linked - Remove Npc Flags'),
(253002,0,3,4,'',61,0,100,0,0,0,0,0,0,'',29,2,0,253284,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'Fillion Flamebreeze - Linked - Follow Invoker Until Fillion Flamebreeze (253284)'),
(253002,0,4,0,'',61,0,100,0,0,0,0,0,0,'',22,1,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Fillion Flamebreeze - Linked - Set Event Phase 1'),
(253002,0,5,0,'',1,1,100,0,600000,600000,0,0,0,'',41,0,30,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Fillion Flamebreeze - Phase 1 Out Of Combat 10 Minutes - Despawn (Respawn 30 s), player gave up'),
(253002,0,6,0,'',65,0,100,0,0,0,0,0,0,'',33,253281,0,0,0,0,0,0,'',12,1,0,0,0,'',0,0,0,0,'Fillion Flamebreeze - On Follow Completed - Kill Credit Stored Player (he then despawns, respawn 120 s)');

DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 15 AND `SourceGroup` = 40877 AND `SourceEntry` = 0;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
(15,40877,0,0,0,9,0,92849,0,0,'',0,0,0,'','Pick him up and carry him - The Missing Scholar taken');

-- 2) Falaath Village (92529) says "If you lose your Mark of Akir ..." but nothing ever gave Mark of Akir (1254832: faction 3577
--    disguise + hidden quest 92542), and Missionary Jasaan's (257065, gossip 41761) "I seem to have lost my mark of Akir" did
--    nothing. Sania gives the mark when 92529 is accepted, and Jasaan gives it back while 92529 is taken or done.
INSERT INTO `quest_template_addon` (`ID`,`SourceSpellID`) VALUES (92529,1254832) ON DUPLICATE KEY UPDATE `SourceSpellID` = 1254832;
UPDATE `creature_template` SET `AIName` = 'SmartAI' WHERE `entry` = 257065;
DELETE FROM `smart_scripts` WHERE `entryorguid` = 257065 AND `source_type` = 0;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`Difficulties`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`event_param_string`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`action_param7`,`action_param_string`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_param4`,`target_param_string`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(257065,0,0,1,'',62,0,100,0,41761,0,0,0,0,'',85,1254832,2,0,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'Missionary Jasaan - On Gossip Option 0 Selected - Invoker Casts Mark of Akir'),
(257065,0,1,0,'',61,0,100,0,0,0,0,0,0,'',72,0,0,0,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'Missionary Jasaan - Linked - Close Gossip');

DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 15 AND `SourceGroup` = 41761 AND `SourceEntry` = 0;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
(15,41761,0,0,0,8,0,92529,0,0,'',0,0,0,'','Lost my mark of Akir - Falaath Village done'),
(15,41761,0,0,0,1,0,1254832,0,0,'',1,0,0,'','Lost my mark of Akir - no Mark of Akir'),
(15,41761,0,0,1,9,0,92529,0,0,'',0,0,0,'','Lost my mark of Akir - Falaath Village taken'),
(15,41761,0,0,1,1,0,1254832,0,0,'',1,0,0,'','Lost my mark of Akir - no Mark of Akir');
