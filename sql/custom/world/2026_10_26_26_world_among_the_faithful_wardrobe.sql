-- 92528 Among the Faithful (Skyborne Horde, Zephras): "Learn about the cultists' plans" asks you to hide in the glowing
-- Wardrobe (254128) by Skypriest Aanders and listen in. The Wardrobe had no spell-click flag, no click spell and no script,
-- so it glowed but could not be used. 70532 data: click spell 1261177 "Eavesdrop" (control vehicle, the Wardrobe has
-- vehicle 9200) and credit spell 1254871 "Eavesdropped" (kill credit 252078). Hide the player, give the credit after a
-- short listen, then let them out.
UPDATE `creature_template` SET `npcflag`=`npcflag`|0x1000000, `AIName`='SmartAI' WHERE `entry`=254128 AND `ScriptName`='';
DELETE FROM `npc_spellclick_spells` WHERE `npc_entry`=254128;
INSERT INTO `npc_spellclick_spells` (`npc_entry`,`spell_id`,`cast_flags`,`user_type`) VALUES
(254128,1261177,1,0);
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=18 AND `SourceGroup`=254128;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`NegativeCondition`,`Comment`) VALUES
(18,254128,1261177,0,0,47,0,92528,8,0,0,'Wardrobe: Eavesdrop only while Among the Faithful is incomplete');
DELETE FROM `smart_scripts` WHERE `entryorguid`=254128 AND `source_type`=0;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(254128,0,0,1,73,0,100,0,0,0,0,0,64,1,0,0,0,0,0,7,0,0,0,0,0,0,0,'Wardrobe - On Spellclick - Store Clicker'),
(254128,0,1,0,61,0,100,0,0,0,0,0,67,1,8000,8000,0,0,100,1,0,0,0,0,0,0,0,'Wardrobe - On Link - Create Timed Event 1 (listen)'),
(254128,0,2,3,59,0,100,0,1,0,0,0,33,252078,0,0,0,0,0,12,1,0,0,0,0,0,0,'Wardrobe - On Timed Event 1 - Quest Credit Learn about the cultists'' plans'),
(254128,0,3,0,61,0,100,0,0,0,0,0,28,1261177,0,0,0,0,0,12,1,0,0,0,0,0,0,'Wardrobe - On Link - Remove Eavesdrop (leave the wardrobe)');
