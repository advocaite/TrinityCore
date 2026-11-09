-- WoW Forever client 1.60.1.70862 (Battle.net update of 7 November 2026, morning UK).
-- Realm 70 stays on gamebuild 70707. A 70862 client sees and joins it when bnetserver.conf has 70862 in Realm.AlsoAcceptBuilds
-- (the repack's start script writes it from repack\client-builds.txt); this row lets bnetserver and worldserver accept that client.
-- Its message numbers are handled per connection: 70862 clients get the 70790 numbering (Classic.Build70790.FirstBuild = 70790
-- covers every later build) until a 70862 change is known.
-- No auth key is known for 70862: keep Network.SkipBuildAuthKeyCheck = 1 in worldserver.conf.
-- bnetserver and worldserver read build_info only at startup: restart both once after this update. Safe to run again.
DELETE FROM `build_info` WHERE `build`=70862;
INSERT INTO `build_info` (`build`,`majorVersion`,`minorVersion`,`bugfixVersion`,`hotfixVersion`) VALUES
(70862,1,60,1,NULL);
