-- WoW Forever client 1.60.1.70862 (Battle.net update of 2026-11-07). Same DB2 layouts as 70790 (all tables the server loads
-- checked against the extracted client data). The Forever realms 70-73 and the bots realm 74 move to 70862. To let 70790
-- clients keep joining them, list that build in bnetserver.conf: Realm.AlsoAcceptBuilds = "70790".
-- No auth key is known for 70862: keep Network.SkipBuildAuthKeyCheck = 1 in worldserver.conf.
-- bnetserver and worldserver read build_info only at startup: restart both after this update. Safe to run again.
DELETE FROM `build_info` WHERE `build`=70862;
INSERT INTO `build_info` (`build`,`majorVersion`,`minorVersion`,`bugfixVersion`,`hotfixVersion`) VALUES
(70862,1,60,1,NULL);

UPDATE `realmlist` SET `gamebuild`=70862 WHERE `id` IN (70,71,72,73,74) AND `gamebuild` IN (70634,70658,70790);
