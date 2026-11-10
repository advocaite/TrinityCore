-- Karrz1/wow-foverver: Karrz moves the realm from client 1.60.1.70205 to 1.60.1.70291 (decided 2026-10-09). The realm list only offers a
-- realm to a client whose build equals realmlist.gamebuild, and Classic.OpcodeGroupShift (-1 = from the realm build) switches the 70291
-- opcode numbering and update field layout on from 70291. Runs after 2026_10_07_99_auth_pin_client_70205.sql. Safe to run again.
DELETE FROM `build_info` WHERE `build`=70291;
INSERT INTO `build_info` (`build`,`majorVersion`,`minorVersion`,`bugfixVersion`,`hotfixVersion`) VALUES
(70291,1,60,1,NULL);

UPDATE `realmlist` SET `gamebuild`=70291 WHERE `id` IN (70,71,72,73);
