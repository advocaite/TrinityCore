-- WoW Forever client 1.60.1.70575 (Battle.net patched the "WoW Forever" game, _forever_\Wow.exe, from 70532 to 70575).
-- Realm 70 stays on gamebuild 70532. A 70575 client sees and joins it only when bnetserver.conf has Realm.AlsoAcceptBuilds = "70575";
-- this row lets the world server accept that client (no auth key is known: keep Network.SkipBuildAuthKeyCheck = 1).
-- bnetserver and worldserver read build_info only at startup: restart both once after this update. Safe to run again.
DELETE FROM `build_info` WHERE `build`=70575;
INSERT INTO `build_info` (`build`,`majorVersion`,`minorVersion`,`bugfixVersion`,`hotfixVersion`) VALUES
(70575,1,60,1,NULL);
