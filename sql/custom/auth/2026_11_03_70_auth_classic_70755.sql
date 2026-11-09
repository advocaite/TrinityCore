-- WoW Forever client 1.60.1.70755 (Battle.net pre-install of 3 November 2026, ahead of the 4 November launch).
-- Realm 70 stays on gamebuild 70707. A 70755 client sees and joins it when bnetserver.conf has 70755 in Realm.AlsoAcceptBuilds
-- (the repack's start script writes it from repack\client-builds.txt); this row lets bnetserver and worldserver accept that client.
-- Its moved message numbers are handled per connection (Classic.Build70755.* in worldserver.conf).
-- No auth key is known for 70755: keep Network.SkipBuildAuthKeyCheck = 1 in worldserver.conf.
-- bnetserver and worldserver read build_info only at startup: restart both once after this update. Safe to run again.
DELETE FROM `build_info` WHERE `build`=70755;
INSERT INTO `build_info` (`build`,`majorVersion`,`minorVersion`,`bugfixVersion`,`hotfixVersion`) VALUES
(70755,1,60,1,NULL);
