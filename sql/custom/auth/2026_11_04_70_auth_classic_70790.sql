-- WoW Forever client 1.60.1.70790 (Battle.net update of 4 November 2026, 18:49 UK, a few hours before the launch).
-- Realm 70 stays on gamebuild 70707. A 70790 client sees and joins it when bnetserver.conf has 70790 in Realm.AlsoAcceptBuilds
-- (the repack's start script writes it from repack\client-builds.txt); this row lets bnetserver and worldserver accept that client.
-- Its message numbers are handled per connection (Classic.Build70755.* and Classic.Build70790.* in worldserver.conf).
-- No auth key is known for 70790: keep Network.SkipBuildAuthKeyCheck = 1 in worldserver.conf.
-- bnetserver and worldserver read build_info only at startup: restart both once after this update. Safe to run again.
DELETE FROM `build_info` WHERE `build`=70790;
INSERT INTO `build_info` (`build`,`majorVersion`,`minorVersion`,`bugfixVersion`,`hotfixVersion`) VALUES
(70790,1,60,1,NULL);
