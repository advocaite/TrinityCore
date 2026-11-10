-- WoW Forever client 1.60.1.70840 (Battle.net update of 7 November 2026, about 04:00 UK).
-- Realm 70 stays on gamebuild 70707. A 70840 client sees and joins it when bnetserver.conf has 70840 in Realm.AlsoAcceptBuilds
-- (the repack's start script writes it from repack\client-builds.txt); this row lets bnetserver and worldserver accept that client.
-- Its message numbers are handled per connection: 70840 clients get the 70790 numbering (Classic.Build70790.FirstBuild = 70790
-- covers every later build) until a 70840 change is known.
-- No auth key is known for 70840: keep Network.SkipBuildAuthKeyCheck = 1 in worldserver.conf.
-- bnetserver and worldserver read build_info only at startup: restart both once after this update. Safe to run again.
DELETE FROM `build_info` WHERE `build`=70840;
INSERT INTO `build_info` (`build`,`majorVersion`,`minorVersion`,`bugfixVersion`,`hotfixVersion`) VALUES
(70840,1,60,1,NULL);
