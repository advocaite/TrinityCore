-- WoW Forever client 1.60.1.70532 (Battle.net game "WoW Forever", _forever_\Wow.exe; the beta WowB.exe is on the same build).
-- Battle.net no longer offers 70450, so the Forever realm 70 moves to 70532 and only accepts 70532 clients afterwards.
-- No auth key is known for 70532: keep Network.SkipBuildAuthKeyCheck = 1 in worldserver.conf.
-- bnetserver reads build_info only at startup: restart it once after this update. Safe to run again.
DELETE FROM `build_info` WHERE `build`=70532;
INSERT INTO `build_info` (`build`,`majorVersion`,`minorVersion`,`bugfixVersion`,`hotfixVersion`) VALUES
(70532,1,60,1,NULL);

UPDATE `realmlist` SET `gamebuild`=70532 WHERE `id`=70;

-- The 70532 test realm 74 (added by hand for the client test) is no longer needed: realm 70 serves 70532 itself.
DELETE FROM `realmcharacters` WHERE `realmid`=74;
DELETE FROM `realmlist` WHERE `id`=74;
