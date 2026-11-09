-- Zephras Isle: The Fate of a Loved One (94897) needs Resaan's Heirloom (266434) from Resaan Nimbuswalker (259013), but his gossip
-- option "Retrieve the heirloom." (menu 42472) had no action, so it gave nothing, and he stood although he is Ealaane's dead
-- husband. He now lies dead and the option gives the heirloom while the quest is taken and the heirloom is not in the bags.
INSERT INTO `creature_template_addon` (`entry`,`StandState`) VALUES (259013,7) ON DUPLICATE KEY UPDATE `StandState` = 7;

UPDATE `creature_template` SET `AIName` = 'SmartAI' WHERE `entry` = 259013;
DELETE FROM `smart_scripts` WHERE `entryorguid` = 259013 AND `source_type` = 0;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`Difficulties`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`event_param_string`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`action_param7`,`action_param_string`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_param4`,`target_param_string`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(259013,0,0,1,'',62,0,100,0,42472,0,0,0,0,'',56,266434,1,0,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'Resaan Nimbuswalker - On Gossip Option 0 Selected - Add Item Resaan''s Heirloom'),
(259013,0,1,0,'',61,0,100,0,0,0,0,0,0,'',72,0,0,0,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'Resaan Nimbuswalker - Linked - Close Gossip');

-- the option shows only while the quest is taken and the heirloom is not in the bags
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 15 AND `SourceGroup` = 42472 AND `SourceEntry` = 0;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
(15,42472,0,0,0,9,0,94897,0,0,'',0,0,0,'','Retrieve the heirloom - The Fate of a Loved One taken'),
(15,42472,0,0,0,2,0,266434,1,0,'',1,0,0,'','Retrieve the heirloom - Resaan''s Heirloom not in the bags');
