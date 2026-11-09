-- Call of Fire (97257, shaman, Shen'dar Highlands): could not be finished.
-- Ids cross-checked with the Questie Forever data (Questie team, Questie/QuestieDB, GPLv3): ender 252382,
-- objective creatures 268764 (ritual) and 268679 (Brazier of Eternal Flame at Valanaar).
--
-- What already works: Olariaan's torch (start item 277329) casts 1302112 "Bearing the Flame" when used within
-- 10 yd of the Eternal Flame spell focus 661086 at the Brazier of Offering: dummy aura on the player plus
-- Kill Credit 268764 ("Complete the Ritual with Olariaan").
-- What was missing: nothing gave "Light the Brazier of Eternal Flame" (268679). The brazier creature at
-- Valanaar now credits a player who comes within 5 yd while still carrying the flame aura and the quest,
-- then takes the aura away. The quest's own 6 minute timer stays the time limit.
UPDATE `creature_template` SET `AIName`='SmartAI' WHERE `entry`=268679 AND `AIName`='' AND `ScriptName`='';
DELETE FROM `smart_scripts` WHERE `entryorguid`=268679 AND `source_type`=0;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_param4`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(268679,0,0,1,10,0,100,0,2,5,1000,1000,1,33,268679,0,0,0,0,0,7,0,0,0,0,0,0,0,0,'Brazier of Eternal Flame - Player in 5 yd with Bearing the Flame - Quest Credit Light the Brazier (Call of Fire)'),
(268679,0,1,0,61,0,100,0,0,0,0,0,0,28,1302112,0,0,0,0,0,7,0,0,0,0,0,0,0,0,'Brazier of Eternal Flame - Linked - Remove Bearing the Flame');
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=22 AND `SourceGroup`=1 AND `SourceEntry`=268679 AND `SourceId`=0;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
(22,1,268679,0,0,1,0,1302112,0,0,0,0,0,'','Brazier of Eternal Flame credit: player has Bearing the Flame'),
(22,1,268679,0,0,9,0,97257,0,0,0,0,0,'','Brazier of Eternal Flame credit: player has Call of Fire (97257)');
