-- Classic 1.60: VMaNGOS EventAI lines whose conditions the converter dropped (2026_09_28_15)
--
-- 1. 36 casters and hunters step back from a rooted victim that stands within 8 yards (VMaNGOS condition 2428 with
--    movement type DISTANCING). Uses the new SMART_EVENT_VICTIM_ROOTED (91) and SMART_ACTION_MOVE_AWAY_FROM_TARGET (160).
--    Archmage Ataeric keeps his Frost Nova trigger (TARGET_BUFFED 512), Defias Taskmaster uses RANGE 0-8.
--    Spirit of Veng 13738 and Spirit of Maraudos 13739 have no AI and no spawns, so they were left alone.
-- 2. Lines the converter kept but ran unconditionally: Bloodmage Thalnos, Darkmaster Gandling, Gerard Abernathy,
--    Witch Doctor Zum'rah, Baron Rivendare, Krakle's Thermometer, Blackhand Thug, Son of Flame, Cenarion Hold Infantry,
--    Expeditionary Mountaineer, Necro Stalker and Venom Stalker. The bowmen were fixed in 2026_10_25_43.

DELETE FROM `smart_scripts` WHERE `source_type` = 0 AND `entryorguid` IN (95,595,596,599,1487,1889,2120,2184,2276,2387,2428,2543,2573,3272,3394,3436,4093,4417,4460,4494,4716,4856,5251,6195,7358,7726,8283,9038,9041,9717,9736,11794,11883,12242,12243,15213) AND `comment` IN ('VMaNGOS EventAI 9503','VMaNGOS EventAI 59502','VMaNGOS EventAI 59601','VMaNGOS EventAI 59901','VMaNGOS EventAI 148701','VMaNGOS EventAI 188901','VMaNGOS EventAI 212002','VMaNGOS EventAI 218401','VMaNGOS EventAI 227601','VMaNGOS EventAI 238705','VMaNGOS EventAI 242801','VMaNGOS EventAI 254302','VMaNGOS EventAI 257303','VMaNGOS EventAI 327202','VMaNGOS EventAI 339402','VMaNGOS EventAI 343601','VMaNGOS EventAI 409302','VMaNGOS EventAI 441701','VMaNGOS EventAI 446001','VMaNGOS EventAI 449402','VMaNGOS EventAI 471603','VMaNGOS EventAI 485602','VMaNGOS EventAI 525103','VMaNGOS EventAI 619505','VMaNGOS EventAI 735806','VMaNGOS EventAI 772607','VMaNGOS EventAI 828301','VMaNGOS EventAI 903801','VMaNGOS EventAI 904103','VMaNGOS EventAI 971702','VMaNGOS EventAI 973602','VMaNGOS EventAI 1179401','VMaNGOS EventAI 1188301','VMaNGOS EventAI 1224202','VMaNGOS EventAI 1224302','VMaNGOS EventAI 1521301');
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`Difficulties`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`event_param_string`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`action_param7`,`action_param_string`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_param4`,`target_param_string`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(95,0,4,0,'',91,0,100,0,15000,15000,0,0,0,'',160,15,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 9503'),
(595,0,4,0,'',91,0,100,0,10000,10000,0,0,0,'',160,15,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 59502'),
(596,0,5,0,'',91,0,100,0,30000,30000,0,0,0,'',160,12,20,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 59601'),
(599,0,5,0,'',91,0,100,0,30000,30000,0,0,0,'',160,12,20,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 59901'),
(1487,0,2,0,'',91,0,100,0,10000,10000,0,0,0,'',160,12,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 148701'),
(1889,0,5,0,'',91,0,100,0,15000,15000,0,0,0,'',160,12,20,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 188901'),
(2120,0,7,0,'',24,0,100,0,512,1,15000,15000,0,'',160,15,20,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 212002'),
(2184,0,3,0,'',91,0,100,0,20000,20000,0,0,0,'',160,20,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 218401'),
(2276,0,3,0,'',91,0,100,0,15000,15000,0,0,0,'',160,15,5,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 227601'),
(2387,0,5,0,'',91,0,100,0,10000,10000,0,0,0,'',160,12,20,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 238705'),
(2428,0,3,0,'',91,0,100,0,10000,10000,0,0,0,'',160,12,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 242801'),
(2543,0,5,0,'',91,0,100,0,10000,10000,0,0,0,'',160,12,20,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 254302'),
(2573,0,4,0,'',91,0,100,0,20000,20000,0,0,0,'',160,12,20,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 257303'),
(3272,0,3,0,'',91,0,100,0,20000,20000,0,0,0,'',160,12,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 327202'),
(3394,0,3,0,'',91,0,100,0,15000,15000,0,0,0,'',160,15,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 339402'),
(3436,0,4,0,'',91,0,100,0,20000,20000,0,0,0,'',160,15,20,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 343601'),
(4093,0,3,0,'',91,0,100,0,20000,20000,0,0,0,'',160,12,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 409302'),
(4417,0,2,0,'',9,0,100,0,0,8,15000,20000,0,'',160,20,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 441701'),
(4460,0,4,0,'',91,0,100,0,20000,20000,0,0,0,'',160,12,20,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 446001'),
(4494,0,4,0,'',91,0,100,0,20000,20000,0,0,0,'',160,12,20,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 449402'),
(4716,0,3,0,'',91,0,100,0,20000,20000,0,0,0,'',160,12,20,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 471603'),
(4856,0,3,0,'',91,0,100,0,20000,20000,0,0,0,'',160,12,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 485602'),
(5251,0,4,0,'',91,0,15,0,20000,20000,0,0,0,'',160,15,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 525103'),
(6195,0,14,0,'',91,4093,100,0,10000,10000,0,0,0,'',160,12,20,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 619505'),
(7358,0,13,0,'',91,0,100,0,10000,10000,0,0,0,'',160,15,20,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 735806'),
(7726,0,17,0,'',91,4089,100,0,20000,20000,0,0,0,'',160,12,20,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 772607'),
(8283,0,3,0,'',91,0,100,0,10000,10000,0,0,0,'',160,15,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 828301'),
(9038,0,6,0,'',91,0,100,0,30000,30000,0,0,0,'',160,12,5,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 903801'),
(9041,0,5,0,'',91,0,100,0,15000,15000,0,0,0,'',160,12,20,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 904103'),
(9717,0,6,0,'',91,0,100,1,15000,15000,0,0,0,'',160,15,20,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 971702'),
(9736,0,4,0,'',91,0,100,0,10000,10000,0,0,0,'',160,12,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 973602'),
(11794,0,2,0,'',91,0,100,0,10000,10000,0,0,0,'',160,12,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 1179401'),
(11883,0,5,0,'',91,0,100,0,10000,10000,0,0,0,'',160,15,20,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 1188301'),
(12242,0,4,0,'',91,0,100,0,10000,10000,0,0,0,'',160,15,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 1224202'),
(12243,0,4,0,'',91,0,100,0,10000,10000,0,0,0,'',160,15,0,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 1224302'),
(15213,0,5,0,'',91,0,100,0,10000,10000,0,0,0,'',160,15,20,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 1521301');
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 22 AND `SourceEntry` IN (95,595,596,599,1487,1889,2120,2184,2276,2387,2428,2543,2573,3272,3394,3436,4093,4417,4460,4494,4716,4856,5251,6195,7358,7726,8283,9038,9041,9717,9736,11794,11883,12242,12243,15213) AND `Comment` LIKE '% (VMaNGOS condition 2428)';
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
(22,5,95,0,0,35,0,1,8,4,'',0,0,0,'','Defias Smuggler - victim within 8 yd (VMaNGOS condition 2428)'),
(22,5,595,0,0,35,0,1,8,4,'',0,0,0,'','Bloodscalp Hunter - victim within 8 yd (VMaNGOS condition 2428)'),
(22,6,596,0,0,35,0,1,8,4,'',0,0,0,'','Brainwashed Noble - victim within 8 yd (VMaNGOS condition 2428)'),
(22,6,599,0,0,35,0,1,8,4,'',0,0,0,'','Marisa du\'Paige - victim within 8 yd (VMaNGOS condition 2428)'),
(22,3,1487,0,0,35,0,1,8,4,'',0,0,0,'','Splinter Fist Enslaver - victim within 8 yd (VMaNGOS condition 2428)'),
(22,6,1889,0,0,35,0,1,8,4,'',0,0,0,'','Dalaran Wizard - victim within 8 yd (VMaNGOS condition 2428)'),
(22,8,2120,0,0,35,0,1,8,4,'',0,0,0,'','Archmage Ataeric - victim within 8 yd (VMaNGOS condition 2428)'),
(22,4,2184,0,0,35,0,1,8,4,'',0,0,0,'','Lady Moongazer - victim within 8 yd (VMaNGOS condition 2428)'),
(22,4,2276,0,0,35,0,1,8,4,'',0,0,0,'','Magistrate Henry Maleb - victim within 8 yd (VMaNGOS condition 2428)'),
(22,6,2387,0,0,35,0,1,8,4,'',0,0,0,'','Hillsbrad Councilman - victim within 8 yd (VMaNGOS condition 2428)'),
(22,4,2428,0,0,35,0,1,8,4,'',0,0,0,'','Jailor Marlgen - victim within 8 yd (VMaNGOS condition 2428)'),
(22,6,2543,0,0,35,0,1,8,4,'',0,0,0,'','Archmage Ansirem Runeweaver - victim within 8 yd (VMaNGOS condition 2428)'),
(22,5,2573,0,0,35,0,1,8,4,'',0,0,0,'','Drywhisker Surveyor - victim within 8 yd (VMaNGOS condition 2428)'),
(22,4,3272,0,0,35,0,1,8,4,'',0,0,0,'','Kolkar Wrangler - victim within 8 yd (VMaNGOS condition 2428)'),
(22,4,3394,0,0,35,0,1,8,4,'',0,0,0,'','Barak Kodobane - victim within 8 yd (VMaNGOS condition 2428)'),
(22,5,3436,0,0,35,0,1,8,4,'',0,0,0,'','Kuz - victim within 8 yd (VMaNGOS condition 2428)'),
(22,4,4093,0,0,35,0,1,8,4,'',0,0,0,'','Galak Wrangler - victim within 8 yd (VMaNGOS condition 2428)'),
(22,5,4460,0,0,35,0,1,8,4,'',0,0,0,'','Murkgill Lord - victim within 8 yd (VMaNGOS condition 2428)'),
(22,5,4494,0,0,35,0,1,8,4,'',0,0,0,'','Scarlet Spellbinder - victim within 8 yd (VMaNGOS condition 2428)'),
(22,4,4716,0,0,35,0,1,8,4,'',0,0,0,'','Slitherblade Tidehunter - victim within 8 yd (VMaNGOS condition 2428)'),
(22,4,4856,0,0,35,0,1,8,4,'',0,0,0,'','Stonevault Cave Hunter - victim within 8 yd (VMaNGOS condition 2428)'),
(22,5,5251,0,0,35,0,1,8,4,'',0,0,0,'','Woodpaw Trapper - victim within 8 yd (VMaNGOS condition 2428)'),
(22,15,6195,0,0,35,0,1,8,4,'',0,0,0,'','Spitelash Siren - victim within 8 yd (VMaNGOS condition 2428)'),
(22,14,7358,0,0,35,0,1,8,4,'',0,0,0,'','Amnennar the Coldbringer - victim within 8 yd (VMaNGOS condition 2428)'),
(22,18,7726,0,0,35,0,1,8,4,'',0,0,0,'','Grimtotem Naturalist - victim within 8 yd (VMaNGOS condition 2428)'),
(22,4,8283,0,0,35,0,1,8,4,'',0,0,0,'','Slave Master Blackheart - victim within 8 yd (VMaNGOS condition 2428)'),
(22,7,9038,0,0,35,0,1,8,4,'',0,0,0,'','Seeth\'rel - victim within 8 yd (VMaNGOS condition 2428)'),
(22,6,9041,0,0,35,0,1,8,4,'',0,0,0,'','Warder Stilgiss - victim within 8 yd (VMaNGOS condition 2428)'),
(22,7,9717,0,0,35,0,1,8,4,'',0,0,0,'','Bloodaxe Summoner - victim within 8 yd (VMaNGOS condition 2428)'),
(22,5,9736,0,0,35,0,1,8,4,'',0,0,0,'','Quartermaster Zigris - victim within 8 yd (VMaNGOS condition 2428)'),
(22,3,11794,0,0,35,0,1,8,4,'',0,0,0,'','Sister of Celebrian - victim within 8 yd (VMaNGOS condition 2428)'),
(22,6,11883,0,0,35,0,1,8,4,'',0,0,0,'','Twilight Master - victim within 8 yd (VMaNGOS condition 2428)'),
(22,5,12242,0,0,35,0,1,8,4,'',0,0,0,'','Spirit of Maraudos - victim within 8 yd (VMaNGOS condition 2428)'),
(22,5,12243,0,0,35,0,1,8,4,'',0,0,0,'','Spirit of Veng - victim within 8 yd (VMaNGOS condition 2428)'),
(22,6,15213,0,0,35,0,1,8,4,'',0,0,0,'','Twilight Overlord - victim within 8 yd (VMaNGOS condition 2428)');

-- Bloodmage Thalnos summons Scorn only while the Scourge Invasion runs
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 22 AND `SourceGroup` = 1 AND `SourceEntry` = 4543 AND `SourceId` = 0;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
(22,1,4543,0,0,12,0,81,0,0,'',0,0,0,'','Bloodmage Thalnos - Scourge Invasion active (VMaNGOS condition 14693)');

-- Darkmaster Gandling: the greeting always plays, the Winter Veil costume only during Winter Veil
UPDATE `smart_scripts` SET `link` = 0, `action_type` = 1, `action_param1` = 0, `action_param2` = 0, `action_param3` = 0 WHERE `entryorguid` = 1853 AND `source_type` = 0 AND `id` = 0;
UPDATE `smart_scripts` SET `event_type` = 63, `event_flags` = 1, `action_type` = 11, `action_param1` = 26200, `action_param2` = 2, `action_param3` = 0 WHERE `entryorguid` = 1853 AND `source_type` = 0 AND `id` = 1;
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 22 AND `SourceGroup` IN (1,2) AND `SourceEntry` = 1853 AND `SourceId` = 0;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
(22,2,1853,0,0,12,0,2,0,0,'',0,0,0,'','Darkmaster Gandling - Winter Veil active (VMaNGOS condition 110)');

-- Gerard Abernathy talks only while Lisa Mason is next to him
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 22 AND `SourceGroup` = 1 AND `SourceEntry` = 5696 AND `SourceId` = 0;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
(22,1,5696,0,0,29,1,5697,5,0,'',0,0,0,'','Gerard Abernathy - Lisa within 5 yd (VMaNGOS condition 5696)');

-- Witch Doctor Zum'rah: the pre-1.12 extra zombie summon is gone; his yell on the zombie call fires 30% of the time
DELETE FROM `smart_scripts` WHERE `entryorguid` = 7271 AND `source_type` = 0 AND `id` = 5;
UPDATE `smart_scripts` SET `link` = 0 WHERE `entryorguid` = 7271 AND `source_type` = 0 AND `id` = 4;
UPDATE `smart_scripts` SET `event_chance` = 30 WHERE `entryorguid` = 950001245 AND `source_type` = 9 AND `id` = 0;

-- Baron Rivendare: the Aurius parts of his aggro and evade scripts need the Aurius event
UPDATE `smart_scripts` SET `link` = 0 WHERE `entryorguid` = 10440 AND `source_type` = 0 AND `id` IN (0,2);
UPDATE `smart_scripts` SET `event_type` = 4, `event_flags` = 1 WHERE `entryorguid` = 10440 AND `source_type` = 0 AND `id` = 1;
UPDATE `smart_scripts` SET `event_type` = 7, `event_flags` = 1 WHERE `entryorguid` = 10440 AND `source_type` = 0 AND `id` = 3;
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 22 AND `SourceGroup` IN (1,4) AND `SourceEntry` = 10440 AND `SourceId` = 0;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
(22,1,10440,0,0,13,1,7,1,0,'',0,0,0,'','Baron Rivendare - Aurius event started (VMaNGOS condition 3756)'),
(22,4,10440,0,0,13,1,7,1,0,'',0,0,0,'','Baron Rivendare - Aurius event started (VMaNGOS condition 3756)');

-- Krakle's Thermometer: each of the five thermometers gives its own reading, the hot one gives the credit
DELETE FROM `smart_scripts` WHERE `entryorguid` IN (10541,-20023712,-20023713,-20023714,-20023715,-20023716) AND `source_type` = 0;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`Difficulties`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`event_param_string`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`action_param7`,`action_param_string`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_param4`,`target_param_string`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(-20023716,0,0,1,'',8,0,100,0,16378,0,10000,10000,0,'',1,0,0,1,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 1054101 (VMaNGOS condition 23716)'),
(-20023716,0,1,0,'',61,0,100,0,0,0,0,0,0,'',33,10541,0,0,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 1054101 (VMaNGOS condition 23716)'),
(-20023714,0,0,0,'',8,0,100,0,16378,0,10000,10000,0,'',1,1,0,1,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 1054101 (VMaNGOS condition 23714)'),
(-20023712,0,0,0,'',8,0,100,0,16378,0,10000,10000,0,'',1,2,0,1,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 1054101 (VMaNGOS condition 23712)'),
(-20023715,0,0,0,'',8,0,100,0,16378,0,10000,10000,0,'',1,3,0,1,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 1054101 (VMaNGOS condition 23715)'),
(-20023713,0,0,0,'',8,0,100,0,16378,0,10000,10000,0,'',1,4,0,1,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 1054101 (VMaNGOS condition 23713)');

-- Blackhand Thug: Awbee becomes a quest giver only when the last thug dies
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 22 AND `SourceGroup` = 1 AND `SourceEntry` = 10762 AND `SourceId` = 0;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
(22,1,10762,0,0,29,1,10762,10,0,'',1,0,0,'','Blackhand Thug - no living thug within 10 yd (VMaNGOS condition 347)');

-- Son of Flame and Cenarion Hold Infantry summon only a victim they cannot reach
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 22 AND `SourceGroup` = 1 AND `SourceEntry` IN (12143,15184) AND `SourceId` = 0;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
(22,1,12143,0,0,61,1,0,0,0,'',0,0,0,'','Son of Flame - cannot reach victim (VMaNGOS condition 84)'),
(22,1,15184,0,0,61,1,0,0,0,'',0,0,0,'','Cenarion Hold Infantry - cannot reach victim (VMaNGOS condition 84)');

-- Expeditionary Mountaineer: the idle emotes belong to single spawns at the Ironforge Airfield
DELETE FROM `smart_scripts` WHERE `entryorguid` IN (14390,-20054000,-20054001,-20301770,-20301774) AND `source_type` = 0;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`Difficulties`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`event_param_string`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`action_param7`,`action_param_string`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_param4`,`target_param_string`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(14390,0,5,0,'',2,0,100,1,0,20,0,0,0,'',25,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 1439001'),
(14390,0,6,0,'',0,0,100,0,0,0,2000,4000,0,'',11,6660,64,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS spell list 143900'),
(-20301774,0,0,0,'',1,0,100,0,1000,7000,6000,11000,0,'',5,7,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 301774 (VMaNGOS condition 1678803)'),
(-20301774,0,1,0,'',2,0,100,1,0,20,0,0,0,'',25,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 1439001'),
(-20301774,0,2,0,'',0,0,100,0,0,0,2000,4000,0,'',11,6660,64,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS spell list 143900'),
(-20054000,0,0,0,'',1,0,100,0,1000,7000,6000,11000,0,'',5,1,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 301774 (VMaNGOS condition 1678804)'),
(-20054000,0,1,0,'',2,0,100,1,0,20,0,0,0,'',25,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 1439001'),
(-20054000,0,2,0,'',0,0,100,0,0,0,2000,4000,0,'',11,6660,64,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS spell list 143900'),
(-20301770,0,0,0,'',1,0,100,0,1000,7000,6000,11000,0,'',5,1,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 301774 (VMaNGOS condition 1678804)'),
(-20301770,0,1,0,'',2,0,100,1,0,20,0,0,0,'',25,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 1439001'),
(-20301770,0,2,0,'',0,0,100,0,0,0,2000,4000,0,'',11,6660,64,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS spell list 143900'),
(-20054001,0,0,1,'',1,0,100,0,20000,40000,20000,40000,0,'',89,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 301775 (VMaNGOS condition 1678805)'),
(-20054001,0,1,2,'',61,0,100,0,0,0,0,0,0,'',90,1,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 301775 (VMaNGOS condition 1678805)'),
(-20054001,0,2,0,'',61,0,100,0,0,0,0,0,0,'',5,7,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 301775 (VMaNGOS condition 1678805)'),
(-20054001,0,3,4,'',1,0,100,0,10000,20000,10000,20000,0,'',90,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 301776 (VMaNGOS condition 1678805)'),
(-20054001,0,4,0,'',61,0,100,0,0,0,0,0,0,'',89,5,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 301776 (VMaNGOS condition 1678805)'),
(-20054001,0,5,0,'',2,0,100,1,0,20,0,0,0,'',25,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'VMaNGOS EventAI 1439001'),
(-20054001,0,6,0,'',0,0,100,0,0,0,2000,4000,0,'',11,6660,64,0,0,0,0,0,'',2,0,0,0,0,'',0,0,0,0,'VMaNGOS spell list 143900');

-- Necro and Venom Stalkers throw Poison Charge only at a target at least 8 yd away
UPDATE `smart_scripts` SET `target_param4` = 8 WHERE `entryorguid` IN (16453,15976) AND `source_type` = 0 AND `id` = 0;
