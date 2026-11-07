-- Client 1.60.1.70790: the two TACT keys Blizzard released for Forever launch content, sent to clients as TactKey hotfixes.
-- 8283 = D2C028CB7AC14D0B and 8321 = 057DC814574BD5B6 (TactKeyLookup rows are already in the 70790 client data).
-- Without them the client cannot read the records these keys lock, among them Arcanum of Binding (spell 1314330, enchant 8726).
-- Keys from the live hotfix cache of build 70790. hotfix_data ids 970108-970109, UniqueId = 2059862016 + key id as in 2026_09_28_01.
DELETE FROM `tact_key` WHERE `ID` IN (8283,8321);
INSERT INTO `tact_key` (`ID`,`Key1`,`Key2`,`Key3`,`Key4`,`Key5`,`Key6`,`Key7`,`Key8`,`Key9`,`Key10`,`Key11`,`Key12`,`Key13`,`Key14`,`Key15`,`Key16`,`VerifiedBuild`) VALUES
(8283,211,28,116,254,55,5,175,87,72,136,228,162,48,120,84,129,70790),
(8321,88,14,186,23,67,169,40,158,31,189,90,205,155,127,217,155,70790);
DELETE FROM `hotfix_data` WHERE `Id` BETWEEN 970108 AND 970109;
DELETE FROM `hotfix_data` WHERE `TableHash` = 3744420815 AND `RecordId` IN (8283,8321);
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(970108,2059870299,3744420815,8283,1,70790),
(970109,2059870337,3744420815,8321,1,70790);
