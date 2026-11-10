-- MANUAL SCRIPT: run by hand on the auth database for the 70532 capture only. Not in sql/custom on purpose (the updater would add realm 74 everywhere).
-- Afterwards: DELETE FROM `realmlist` WHERE `id`=74;
-- WoW Classic beta / WoW Forever client 1.60.1.70532
-- Adds build 70532 to build_info and a separate TEST realm (id 74) on that build. The Forever realms 70-73 stay on their current build.
-- A realm only lists for and accepts clients of its exact gamebuild, and one worldserver serves one realm, so the 70532 test realm
-- needs its own worldserver (RealmID = 74, WorldServerPort = 8125; see doc/classic70532/worldserver_test70532.conf.example).
-- No auth key is known for 70532 yet: that worldserver needs Network.SkipBuildAuthKeyCheck = 1, as for 70450.
-- Safe to run again.
DELETE FROM `build_info` WHERE `build`=70532;
INSERT INTO `build_info` (`build`,`majorVersion`,`minorVersion`,`bugfixVersion`,`hotfixVersion`) VALUES
(70532,1,60,1,NULL);

INSERT IGNORE INTO `realmlist` (`id`,`name`,`address`,`localAddress`,`localSubnetMask`,`port`,`icon`,`flag`,`timezone`,`allowedSecurityLevel`,`population`,`gamebuild`,`Region`,`Battlegroup`,`contentSetId`) VALUES
(74,'Forever Test 70532','127.0.0.1','127.0.0.1','255.255.255.0',8125,0,0,1,0,0,70532,70,1,137);
