-- Among the Faithful (92528): the Wardrobe (254128) never gave "Learn about the cultists' plans" (tested in game 11-01).
-- Eavesdrop 1261177 is a control vehicle aura, so while the player sits in the Wardrobe the Wardrobe is charmed by the player.
-- SmartAI skips every event of a charmed creature unless the event has SMART_EVENT_FLAG_WHILE_CHARMED (0x200), and the timed
-- event made by row 1 only inherits that flag from the row that creates it. So the 8 second listen timer never fired.
-- Give all rows the flag, and also take the Eavesdrop aura off the Wardrobe itself (the vehicle aura sits on the Wardrobe,
-- not on the player), so the player is let out when the credit is given.
UPDATE `smart_scripts` SET `event_flags` = `event_flags` | 0x200 WHERE `entryorguid` = 254128 AND `source_type` = 0 AND `id` IN (0,1,2,3);
UPDATE `smart_scripts` SET `link` = 4 WHERE `entryorguid` = 254128 AND `source_type` = 0 AND `id` = 3;
DELETE FROM `smart_scripts` WHERE `entryorguid` = 254128 AND `source_type` = 0 AND `id` = 4;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(254128,0,4,0,61,0,100,0x200,0,0,0,0,28,1261177,0,0,0,0,0,1,0,0,0,0,0,0,0,'Wardrobe - On Link - Remove Eavesdrop from self (let the player out)');
