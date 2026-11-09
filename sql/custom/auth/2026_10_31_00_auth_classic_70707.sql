-- WoW Forever client 1.60.1.70707 (Battle.net update of 2026-10-31; EU shows it from the morning).
-- Realm 70 moves to 70707. Clients of 70685, 70658, 70634, 70630 and 70532 still see and join it when bnetserver.conf has
-- Realm.AlsoAcceptBuilds = "70685,70658,70634,70630,70532" (the repack's start script writes it from repack\client-builds.txt).
-- No auth key is known for 70707: keep Network.SkipBuildAuthKeyCheck = 1 in worldserver.conf.
-- bnetserver and worldserver read build_info only at startup: restart both once after this update. Safe to run again.
DELETE FROM `build_info` WHERE `build`=70707;
INSERT INTO `build_info` (`build`,`majorVersion`,`minorVersion`,`bugfixVersion`,`hotfixVersion`) VALUES
(70707,1,60,1,NULL);

UPDATE `realmlist` SET `gamebuild`=70707 WHERE `id`=70;
