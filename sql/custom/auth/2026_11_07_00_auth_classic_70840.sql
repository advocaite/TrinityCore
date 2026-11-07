-- WoW Forever client 1.60.1.70840 (Battle.net update of 2026-11-07). It uses the 70790 message numbers (every 70790 check is
-- "build >= 70790"), so only the build row and the realm build change. Players who have not updated yet can keep joining when
-- bnetserver.conf has Realm.AlsoAcceptBuilds = "70790".
-- No auth key is known for 70840: keep Network.SkipBuildAuthKeyCheck = 1 in worldserver.conf.
-- bnetserver and worldserver read build_info only at startup: restart both after this update. Safe to run again.
DELETE FROM `build_info` WHERE `build`=70840;
INSERT INTO `build_info` (`build`,`majorVersion`,`minorVersion`,`bugfixVersion`,`hotfixVersion`) VALUES
(70840,1,60,1,NULL);

UPDATE `realmlist` SET `gamebuild`=70840 WHERE `id` IN (70,71,72,73,74) AND `gamebuild`=70790;
