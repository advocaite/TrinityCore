-- Legging It 96821 and Bloodtalon Matriarch 97223 (Durotar) have no map area: the quest log cannot show where to go
-- (noobiru, 0.1.35). The 10-01 sniff carried both quests but no quest_poi rows for them. Built here from our own spawns
-- (2026_11_05_99): one area per objective around the spawns of its creature, 15 yards wider, plus the quest ender's spot.
-- Turn-in spots from EG Link (Gar'Thok 3139, Xar'Ti 7953). Data from Efficient Games (https://efficient.games), licensed
-- under CC BY 4.0. INFERRED: live areas are not sniffed, so the shapes are ours, not Blizzard's. Safe to run again.
DELETE FROM `quest_poi` WHERE `QuestID` IN (96821,97223);
INSERT INTO `quest_poi` (`QuestID`,`BlobIndex`,`Idx1`,`ObjectiveIndex`,`QuestObjectiveID`,`QuestObjectID`,`MapID`,`UiMapID`,`Priority`,`Flags`,`WorldEffectID`,`PlayerConditionID`,`NavigationPlayerConditionID`,`SpawnTrackingID`,`AlwaysAllowMergingBlobs`,`VerifiedBuild`) VALUES
(96821,0,0,-1,0,0,1,1411,0,0,0,0,0,0,0,0),
(96821,0,1,0,474459,266850,1,1411,0,0,0,0,0,0,0,0),
(96821,0,2,1,474460,266849,1,1411,0,0,0,0,0,0,0,0),
(97223,0,0,-1,0,0,1,1411,0,0,0,0,0,0,0,0),
(97223,0,1,0,475459,277128,1,1411,0,0,0,0,0,0,0,0);

DELETE FROM `quest_poi_points` WHERE `QuestID` IN (96821,97223);
INSERT INTO `quest_poi_points` (`QuestID`,`Idx1`,`Idx2`,`X`,`Y`,`Z`,`VerifiedBuild`) VALUES
-- Gar'Thok, Razor Hill
(96821,0,0,275,-4710,24,0),
-- Ridgeshade Creeper 266850
(96821,1,0,-456,-4691,37,0),(96821,1,1,-356,-4760,37,0),(96821,1,2,-327,-4761,37,0),(96821,1,3,-268,-4760,37,0),
(96821,1,4,-73,-4692,37,0),(96821,1,5,-45,-4680,37,0),(96821,1,6,4,-4657,37,0),(96821,1,7,40,-4630,37,0),
(96821,1,8,-68,-4609,37,0),(96821,1,9,-105,-4607,37,0),(96821,1,10,-434,-4680,37,0),
-- Ridgeshade Lurker 266849
(96821,2,0,-519,-4702,37,0),(96821,2,1,-447,-4805,37,0),(96821,2,2,-381,-4796,37,0),(96821,2,3,138,-4701,37,0),
(96821,2,4,19,-4623,37,0),(96821,2,5,-187,-4562,37,0),(96821,2,6,-442,-4645,37,0),
-- Xar'Ti, Sen'jin Village
(97223,0,0,-855,-4884,19,0),
-- Bloodtalon Matriarch 268530 (drops Eggs of the Bloodtalon Matriarch 277128)
(97223,1,0,-766,-5591,26,0),(97223,1,1,-727,-5614,26,0),(97223,1,2,-692,-5602,26,0),(97223,1,3,-680,-5574,26,0),
(97223,1,4,-727,-5560,26,0);
