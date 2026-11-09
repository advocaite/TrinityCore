-- Two fixes from the Questie Forever hand-written corrections of 2026-11-02 (Questie team, Questie/QuestieDB,
-- master 31cc4af5, foreverQuestFixes.lua).

-- The Blackwood Corrupted (4763, Felwood): Questie: "Summon Xabraxxis at object 176705 (Bonfire) once you have the
-- items from the Blackwood Stores, then kill 10373". The Filled Cleansing Bowl casts 16072 at the bonfire, which sends
-- event 3938, but nothing in our world answered that event, so Xabraxxis never came and the quest could not be done.
-- Positions and creatures from the vanilla event 3938 (vMaNGOS world, GPL-2): three Blackwood Warriors 2168 and
-- Xabraxxis 10373 by the bonfire at (6875,-489). Vanilla brings Xabraxxis 18 s after the warriors; a send-event
-- script has no timer here, so all four come at once. They despawn 90 s after leaving combat.
DELETE FROM `event_script_names` WHERE `Id`=3938;
INSERT INTO `event_script_names` (`Id`,`ScriptName`) VALUES (3938,'SmartEventTrigger');
DELETE FROM `smart_scripts` WHERE `entryorguid`=3938 AND `source_type`=3;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_param4`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(3938,3,0,0,87,0,100,0,0,0,0,0,0,12,2168,4,90000,0,4,1,8,0,0,0,0,6887.17,-482.53,40.2585,3.45575,'The Blackwood Corrupted: event 3938 - Summon Blackwood Warrior'),
(3938,3,1,0,87,0,100,0,0,0,0,0,0,12,2168,4,90000,0,4,1,8,0,0,0,0,6868.93,-525.996,40.2464,1.48353,'The Blackwood Corrupted: event 3938 - Summon Blackwood Warrior'),
(3938,3,2,0,87,0,100,0,0,0,0,0,0,12,2168,4,90000,0,4,1,8,0,0,0,0,6850.23,-476.544,40.3891,0.418879,'The Blackwood Corrupted: event 3938 - Summon Blackwood Warrior'),
(3938,3,3,0,87,0,100,0,0,0,0,0,0,12,10373,4,90000,0,4,1,8,0,0,0,0,6891.92,-477.762,44.3988,3.80459,'The Blackwood Corrupted: event 3938 - Summon Xabraxxis');

-- Knowledge in the Deeps (971): Questie: disabled by The Powers Below (968). Not offered once 968 is in the log or done.
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=19 AND `SourceGroup`=0 AND `SourceEntry`=971 AND `SourceId`=0 AND `ConditionTypeOrReference` IN (8,9) AND `ConditionValue1`=968;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
(19,0,971,0,0,8,0,968,0,0,1,0,0,'','Knowledge in the Deeps - not after The Powers Below (968) is done'),
(19,0,971,0,0,9,0,968,0,0,1,0,0,'','Knowledge in the Deeps - not while The Powers Below (968) is in the log');
