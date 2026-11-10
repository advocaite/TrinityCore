-- WoW Forever client 1.60.1.70685 (Battle.net update of 2026-10-28, evening).
-- Realm 70 moves to 70685. Clients of 70658, 70634, 70630 and 70532 still see and join it when bnetserver.conf has
-- Realm.AlsoAcceptBuilds = "70658,70634,70630,70532" (the repack's start script writes it from repack\client-builds.txt).
-- No auth key is known for 70685: keep Network.SkipBuildAuthKeyCheck = 1 in worldserver.conf.
-- bnetserver and worldserver read build_info only at startup: restart both once after this update. Safe to run again.
DELETE FROM `build_info` WHERE `build`=70685;
INSERT INTO `build_info` (`build`,`majorVersion`,`minorVersion`,`bugfixVersion`,`hotfixVersion`) VALUES
(70685,1,60,1,NULL);

UPDATE `realmlist` SET `gamebuild`=70685 WHERE `id`=70;
