-- The Sleeper Has Awakened (5321, Darkshore): live has the "Horn of Awakening (13536) 0/1" item objective at storage
-- index 0 (Data from Efficient Games (https://efficient.games), licensed under CC BY 4.0, EG Link 1.60.1.70862,
-- objective 383260). This puts back what 2026_11_08_21 removed. It also makes the horn drop again: Kerlonian's Chest
-- (13964) only gives it to players whose quest needs it (QuestRequired = 1). Safe to run again.
DELETE FROM `quest_objectives` WHERE `QuestID`=5321 AND `Type`=1 AND `ObjectID`=13536;
DELETE FROM `quest_objectives` WHERE `ID`=383260;
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES
(383260,5321,1,0,0,13536,1,0,0,0,0,0,1,NULL,70862);
