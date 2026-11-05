-- WoW Forever client 1.60.1.70790 (2026-11-05). From this build the game is its own Battle.net product: "wowf", installed in
-- World of Warcraft\_forever_ with Wow.exe (before: wow_classic_beta, _classic_beta_\WowB.exe). The Forever realms 70-73 and the
-- bots realm 74 move to 70790. To let older clients keep joining them, list those builds in bnetserver.conf:
-- Realm.AlsoAcceptBuilds = "70658,70634".
-- No auth key is known for 70790: keep Network.SkipBuildAuthKeyCheck = 1 in worldserver.conf.
-- bnetserver and worldserver read build_info only at startup: restart both after this update. Safe to run again.
DELETE FROM `build_info` WHERE `build`=70790;
INSERT INTO `build_info` (`build`,`majorVersion`,`minorVersion`,`bugfixVersion`,`hotfixVersion`) VALUES
(70790,1,60,1,NULL);

UPDATE `realmlist` SET `gamebuild`=70790 WHERE `id` IN (70,71,72,73,74) AND `gamebuild` IN (70450,70532,70630,70634,70658);
