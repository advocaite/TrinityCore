-- Mage city portals that open but take nobody anywhere (Discord, Tornn 2026-10-29: a bot mage's portal does nothing on a click).
-- Portal: Stormwind 10059 and the other five place a Portal to <city> object (176296, 176497-176501, TRANS_DOOR), and a click on it
-- casts its portal effect 17334 / 17607-17611 (TELEPORT_UNITS, TARGET_DEST_DB, client 70658). None of the six had a
-- spell_target_position row, so the cast found no destination and stopped. Each one now leads where the mage's own Teleport to the
-- same city leads (3561, 3562, 3565, 3567, 3566, 3563, already in this table), as the WotLK data pairs them (AzerothCore,
-- azerothcore/azerothcore-wotlk, data/sql/base/db_world/spell_target_position.sql: each portal effect at its teleport's spot).
--   17334 Stormwind <- 3561, 17607 Ironforge <- 3562, 17608 Darnassus <- 3565, 17609 Orgrimmar <- 3567,
--   17610 Thunder Bluff <- 3566, 17611 Undercity <- 3563.
-- Safe to run again.
DELETE FROM `spell_target_position` WHERE `ID` IN (17334,17607,17608,17609,17610,17611);
INSERT INTO `spell_target_position` (`ID`,`EffectIndex`,`OrderIndex`,`MapID`,`PositionX`,`PositionY`,`PositionZ`,`Orientation`,`VerifiedBuild`)
SELECT p.`portal`, 0, 0, t.`MapID`, t.`PositionX`, t.`PositionY`, t.`PositionZ`, t.`Orientation`, 0
FROM (SELECT 17334 AS `portal`, 3561 AS `teleport` UNION ALL SELECT 17607, 3562 UNION ALL SELECT 17608, 3565
      UNION ALL SELECT 17609, 3567 UNION ALL SELECT 17610, 3566 UNION ALL SELECT 17611, 3563) p
JOIN `spell_target_position` t ON t.`ID` = p.`teleport` AND t.`EffectIndex` = 0 AND t.`OrderIndex` = 0;
