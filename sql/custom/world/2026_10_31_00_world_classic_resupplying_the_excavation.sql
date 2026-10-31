-- Resupplying the Excavation (273): its area trigger 171 no longer exists in the client, so the
-- quest can't be completed and Huldar shows an empty dialog. Replaced by an objective credited
-- when the player comes within 20 yards of Huldar.
UPDATE `quest_template` SET `Flags`=8 WHERE `ID`=273;
DELETE FROM `quest_objectives` WHERE `ID`=9000273;
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`Visible`,`Description`,`VerifiedBuild`) VALUES
(9000273,273,0,0,0,2057,1,1,'Find Huldar, Miran, and Saean',70450);
UPDATE `creature_template` SET `AIName`='SmartAI' WHERE `entry`=2057;
DELETE FROM `smart_scripts` WHERE `entryorguid`=2057 AND `source_type`=0;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`action_type`,`action_param1`,`target_type`,`comment`) VALUES
(2057,0,0,0,10,0,100,0,1,20,1000,1000,1,33,2057,7,'Huldar - On player in range - Credit Resupplying the Excavation');
