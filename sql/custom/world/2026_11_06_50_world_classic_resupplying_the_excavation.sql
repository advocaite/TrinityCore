-- Resupplying the Excavation (273, Loch Modan): the quest only completed through area trigger 171, which is not in the
-- 1.60 client's AreaTrigger.db2 (checked in 70685), so it never completed and the turn-in at Huldar (2057) showed nothing.
-- Now the quest has one objective, Huldar 2057, credited when the player comes within 20 yards of him (SmartAI).
-- Fix from Finelam (Trixx34/Trinity-Forever-Modules, branch feature/advocaite-pr263-271, commit 3feca4a7, GPL-2.0).
-- Safe to run twice.
UPDATE `quest_template` SET `Flags` = `Flags` & ~4 WHERE `ID` = 273;
DELETE FROM `areatrigger_involvedrelation` WHERE `id` = 171 AND `quest` = 273;
DELETE FROM `quest_objectives` WHERE `ID` = 9000273;
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`Visible`,`Description`,`VerifiedBuild`) VALUES
(9000273,273,0,0,0,2057,1,1,'Find Huldar, Miran, and Saean',0);
UPDATE `creature_template` SET `AIName` = 'SmartAI' WHERE `entry` = 2057;
DELETE FROM `smart_scripts` WHERE `entryorguid` = 2057 AND `source_type` = 0;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`action_type`,`action_param1`,`target_type`,`comment`) VALUES
(2057,0,0,0,10,0,100,0,1,20,1000,1000,1,33,2057,7,'Huldar - Friendly player within 20 yards - Credit for Resupplying the Excavation');
