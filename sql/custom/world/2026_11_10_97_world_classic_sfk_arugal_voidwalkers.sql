-- Shadowfang Keep (map 33): after Fenrus the Devourer dies, Archmage Arugal summons four Arugal's Voidwalkers (4627, 950001184).
-- classic_instance_shadowfang_keep opens the Sorcerer's Gate (18972) once SetData(6 voidwalker, DONE) has come in four times, but
-- the voidwalkers had no script, so nothing reported their deaths and the way on to Wolf Master Nandos and Arugal stayed shut.
-- On death: instance data 6 = DONE (3). Also the vanilla behaviour: attack at once, Dark Offering (7154) heals a hurt voidwalker.
-- Safe to run again.
UPDATE `creature_template` SET `AIName` = 'SmartAI' WHERE `entry` = 4627;
DELETE FROM `smart_scripts` WHERE `entryorguid` = 4627 AND `source_type` = 0;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,
`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`action_type`,`action_param1`,`action_param2`,`target_type`,`target_param1`,`comment`) VALUES
(4627,0,0,0,54,0,100,0,0,0,0,0,0,49,0,0,21,60,'Arugal''s Voidwalker - Just Summoned - Attack Closest Player'),
(4627,0,1,0,74,0,100,0,1,70,5000,11000,30,11,7154,0,7,0,'Arugal''s Voidwalker - Friendly At 70% Health Within 30 Yards - Cast Dark Offering'),
(4627,0,2,0,6,0,100,0,0,0,0,0,0,34,6,3,1,0,'Arugal''s Voidwalker - On Death - Set Instance Data 6 (voidwalker) to Done');
