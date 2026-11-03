-- WoW Forever client 1.60.1.70707 (Battle.net update of 2026-11-02) and 1.60.1.70755 (2026-11-03). 70707 has the same protocol
-- as 70658. 70755 moved opcodes: two client messages and one server message were added to the main groups, handled in
-- ClassicOpcodes.cpp from realm build 70755 on (Classic.*InsertIndex70755 in worldserver.conf). The Forever realms 70-73 move to
-- 70755; older clients cannot join a 70755 realm (different opcode numbers), so do not list them in Realm.AlsoAcceptBuilds.
-- No auth key is known for 70707 or 70755: keep Network.SkipBuildAuthKeyCheck = 1 in worldserver.conf.
-- bnetserver and worldserver read build_info only at startup: restart both after this update. Safe to run again.
DELETE FROM `build_info` WHERE `build` IN (70707,70755);
INSERT INTO `build_info` (`build`,`majorVersion`,`minorVersion`,`bugfixVersion`,`hotfixVersion`) VALUES
(70707,1,60,1,NULL),
(70755,1,60,1,NULL);

UPDATE `realmlist` SET `gamebuild`=70755 WHERE `id` IN (70,71,72,73) AND `gamebuild` IN (70450,70532,70630,70634,70658,70707);
