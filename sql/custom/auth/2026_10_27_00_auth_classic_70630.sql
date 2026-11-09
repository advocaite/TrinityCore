-- WoW Forever client 1.60.1.70630 (Battle.net patched the WoW Forever game on 2026-10-26).
-- Realm 70 moves to 70630. A 70532 client still sees and joins it when bnetserver.conf has Realm.AlsoAcceptBuilds = "70532".
-- No auth key is known for 70630: keep Network.SkipBuildAuthKeyCheck = 1 in worldserver.conf.
-- bnetserver and worldserver read build_info only at startup: restart both once after this update. Safe to run again.
DELETE FROM `build_info` WHERE `build`=70630;
INSERT INTO `build_info` (`build`,`majorVersion`,`minorVersion`,`bugfixVersion`,`hotfixVersion`) VALUES
(70630,1,60,1,NULL);

UPDATE `realmlist` SET `gamebuild`=70630 WHERE `id`=70;
