-- WoW Classic beta client (wow_classic_beta, WowB.exe) 1.60.1.70450
DELETE FROM `build_info` WHERE `build`=70450;
INSERT INTO `build_info` (`build`,`majorVersion`,`minorVersion`,`bugfixVersion`,`hotfixVersion`) VALUES
(70450,1,60,1,NULL);

-- No auth key is known for Win-x64-WoWB 70450 yet; see Network.SkipBuildAuthKeyCheck in worldserver.conf

-- Realms are not switched here: a realm only lists for and accepts clients of its exact gamebuild, so moving it to 70450
-- locks out 70425 clients. Switch by hand when ready, see doc/Classic70450.md:
-- UPDATE `realmlist` SET `gamebuild`=70450 WHERE `id`=70;
