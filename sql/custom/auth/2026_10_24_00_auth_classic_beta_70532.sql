-- WoW Classic beta client (wow_classic_beta, WowB.exe) 1.60.1.70532
DELETE FROM `build_info` WHERE `build`=70532;
INSERT INTO `build_info` (`build`,`majorVersion`,`minorVersion`,`bugfixVersion`,`hotfixVersion`) VALUES
(70532,1,60,1,NULL);

-- No auth key is known for Win-x64-WoWB 70532 yet; see Network.SkipBuildAuthKeyCheck in worldserver.conf

-- The Forever realms 70-73 move from 70450 to 70532: they only accept 70532 clients afterwards.
-- Other realms and realms on other builds are left alone.
UPDATE `realmlist` SET `gamebuild`=70532 WHERE `id` IN (70,71,72,73) AND `gamebuild`=70450;
