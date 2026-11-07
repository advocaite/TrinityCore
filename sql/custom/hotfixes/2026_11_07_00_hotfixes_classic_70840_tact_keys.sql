-- Client 1.60.1.70840 (Battle.net update of 7 November 2026, about 04:00 UK): three more TACT keys Blizzard released for Forever
-- content, sent to clients as TactKey hotfixes: 8341 = 5699DB626E0CD467, 8342 = D731877A6452D7E6, 8343 = DEFE949FF04343D4.
-- Without them the client cannot read the records these keys lock. Keys from the live hotfix cache of build 70840.
-- hotfix_data ids 970110-970112, UniqueId = 2059862016 + key id as in 2026_09_28_01. Safe to run again.
DELETE FROM `tact_key` WHERE `ID` IN (8341,8342,8343);
INSERT INTO `tact_key` (`ID`,`Key1`,`Key2`,`Key3`,`Key4`,`Key5`,`Key6`,`Key7`,`Key8`,`Key9`,`Key10`,`Key11`,`Key12`,`Key13`,`Key14`,`Key15`,`Key16`,`VerifiedBuild`) VALUES
(8341,136,195,117,196,15,7,194,248,97,119,76,22,230,111,107,109,70840),
(8342,193,212,166,223,193,229,196,155,173,232,251,106,12,49,128,19,70840),
(8343,120,91,196,160,79,98,70,141,141,130,87,23,146,52,114,13,70840);
DELETE FROM `hotfix_data` WHERE `Id` BETWEEN 970110 AND 970112;
DELETE FROM `hotfix_data` WHERE `TableHash` = 3744420815 AND `RecordId` IN (8341,8342,8343);
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(970110,2059870357,3744420815,8341,1,70840),
(970111,2059870358,3744420815,8342,1,70840),
(970112,2059870359,3744420815,8343,1,70840);
