-- Mounts that summon but give no speed and drop off again (70790 client data). These mount spells have no Mount.db2 row, so
-- the server finds no mount type for them: the mount aura gets no speed capability (walk speed) and the next area or skill
-- update removes it. Each gets a Mount row (sent as hotfix) with the Classic ground mount type that fits its item:
--   453 = Riding 75, 60% (level 40 items)   454 = Riding 150, 100% (level 60 epic items)
--   471 = no Riding needed, 60% (the level 25 Trainee's mounts of the Warsong Gulch quartermasters, like the class mounts)
-- Flag 64 = only shown in the mount journal once learned, so the old unused ones never clutter it.
-- Also: Whistle of the Mottled Raptor (mount 3900, 2026_10_04_00) used retail mount type 230, which the 70790 client data
-- does not have, so it said "You can't do that here". It gets type 453, and a new hotfix push so clients drop the old copy.
-- Safe to run again.
DELETE FROM `mount` WHERE `ID` IN (3901,3902,3903,3904,3905,3906,3907,3908,3909,3910,3911,3912,3913,3914,3915,3916,3917,3918,3919,3900);
INSERT INTO `mount` (`Name`,`SourceText`,`Description`,`ID`,`MountTypeID`,`Flags`,`SourceTypeEnum`,`SourceSpellID`,`PlayerConditionID`,
`MountFlyRideHeight`,`UiModelSceneID`,`MountSpecialRiderAnimKitID`,`MountSpecialSpellVisualKitID`,`VerifiedBuild`) VALUES
('Mottled Raptor','','A swift raptor with a mottled hide.',3900,453,0,0,436288,0,0,4,0,0,70790),
('Palomino Stallion','','',3901,453,64,-1,471,0,0,4,0,0,70790),
('Blue Ram','','',3902,453,64,-1,6897,0,0,4,0,0,70790),
('Panther','','',3903,453,64,-1,10787,0,0,4,0,0,70790),
('Leopard','','',3904,453,64,-1,10788,0,0,4,0,0,70790),
('Spotted Panther','','',3905,453,64,-1,10792,0,0,4,0,0,70790),
('Obsidian Raptor','','',3906,453,64,-1,10798,0,0,4,0,0,70790),
('Primal Leopard','','',3907,453,64,-1,16058,0,0,4,0,0,70790),
('Tawny Sabercat','','',3908,453,64,-1,16059,0,0,4,0,0,70790),
('Golden Sabercat','','',3909,453,64,-1,16060,0,0,4,0,0,70790),
('Purple Mechanostrider','','',3910,453,64,-1,17455,0,0,4,0,0,70790),
('Red & Blue Mechanostrider','','',3911,453,64,-1,17456,0,0,4,0,0,70790),
('Fluorescent Green Mechanostrider','','',3912,453,64,-1,17458,0,0,4,0,0,70790),
('Swift Dawnsaber','','',3913,454,64,-1,23220,0,0,4,0,0,70790),
('Chromatic Mount','','',3914,454,64,-1,24576,0,0,4,0,0,70790),
('Riding Gryphon','','',3915,454,64,-1,3363,0,0,4,0,0,70790),
('Skeletal Steed','','',3916,454,64,-1,29059,0,0,4,0,0,70790),
('Trainee''s Sentinel Nightsaber','','',3917,471,64,-1,429856,0,0,4,0,0,70790),
('Trainee''s Outrider Wolf','','',3918,471,64,-1,429857,0,0,4,0,0,70790),
('Swift Spectral Tiger','','',3919,454,64,-1,1214283,0,0,4,0,0,70790);

DELETE FROM `hotfix_data` WHERE `Id` = 984000 OR `Id` BETWEEN 984300 AND 984319 OR (`TableHash` = 2524150337 AND `RecordId` IN (3901,3902,3903,3904,3905,3906,3907,3908,3909,3910,3911,3912,3913,3914,3915,3916,3917,3918,3919,3900));
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(984300,1281443200,2524150337,3901,1,70790),
(984301,1281443201,2524150337,3902,1,70790),
(984302,1281443202,2524150337,3903,1,70790),
(984303,1281443203,2524150337,3904,1,70790),
(984304,1281443204,2524150337,3905,1,70790),
(984305,1281443205,2524150337,3906,1,70790),
(984306,1281443206,2524150337,3907,1,70790),
(984307,1281443207,2524150337,3908,1,70790),
(984308,1281443208,2524150337,3909,1,70790),
(984309,1281443209,2524150337,3910,1,70790),
(984310,1281443210,2524150337,3911,1,70790),
(984311,1281443211,2524150337,3912,1,70790),
(984312,1281443212,2524150337,3913,1,70790),
(984313,1281443213,2524150337,3914,1,70790),
(984314,1281443214,2524150337,3915,1,70790),
(984315,1281443215,2524150337,3916,1,70790),
(984316,1281443216,2524150337,3917,1,70790),
(984317,1281443217,2524150337,3918,1,70790),
(984318,1281443218,2524150337,3919,1,70790),
(984319,1281443219,2524150337,3900,1,70790);

-- Swift Spectral Tiger: the 70790 client has no model for it (display 21974 is not in its CreatureDisplayInfo), so it gets the
-- Swift Zulian Tiger look (display 15290, as its own MountXDisplay), which the client has.
DELETE FROM `mount_x_display` WHERE `ID` = 9901;
INSERT INTO `mount_x_display` (`ID`,`CreatureDisplayInfoID`,`PlayerConditionID`,`Unknown1100`,`MountID`,`VerifiedBuild`) VALUES
(9901,15290,0,0,3919,70790);

DELETE FROM `hotfix_data` WHERE `Id` = 984320 OR (`TableHash` = 2030087241 AND `RecordId` = 9901);
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(984320,1281443220,2030087241,9901,1,70790);
