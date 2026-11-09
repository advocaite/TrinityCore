-- Karrz1/wow-foverver: keeps clients 1.60.1.70205 and 1.60.1.70291 whitelisted in build_info. Releases leave out
-- 2026_10_07_99_auth_pin_client_70205.sql and 2026_10_09_01_auth_forever_move_to_70291.sql (local notes in their comments);
-- their realm build changes are replaced by the later classic_beta files anyway, so this carries only their build_info rows.
-- realmlist is not touched. Safe to run again.
DELETE FROM `build_info` WHERE `build` IN (70205,70291);
INSERT INTO `build_info` (`build`,`majorVersion`,`minorVersion`,`bugfixVersion`,`hotfixVersion`) VALUES
(70205,1,60,1,NULL),
(70291,1,60,1,NULL);
