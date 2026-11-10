-- Achievement 64283 "Played in the WoW: Forever Beta" (2026_10_09_00): the 70291 client has its own record, the 70790 client's
-- Achievement.db2 does not, so its chat line read "has earned the achievement !" with no name. A hotfix_data row (table hash of
-- Achievement) sends the server's record to the client. Safe to run again.
DELETE FROM `hotfix_data` WHERE `Id` = 982131;
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(982131,1280376963,3538824359,64283,1,0);
