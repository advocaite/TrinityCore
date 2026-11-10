-- Classic 1.60 (WoW Forever): fishing base level for the two low-level water zones that had no row.
-- Check of 2026-11-06: all 1.60 AreaTable zones (client 70790) against skill_fishing_base_level. Every vanilla row
-- (MaNGOS Zero world DB, GPL v2) is already there with the same value. With no row the zone skill is 0, so the catch
-- chance is skill + 5%: harmless for skilled fishers, but a new fisher lands only 6%. These two are fished by new players:
--   377 Southfury River (Durotar / Orgrimmar / Barrens side) -> -20 like The Barrens and Orgrimmar
--   170 Lordamere Lake (Silverpine / Hillsbrad side, has fishing loot) -> -20 like Silverpine Forest
-- Only easier than before (skill + 25% instead of skill + 5%). Other zones without a row stay as in vanilla.
-- Safe to run again.
DELETE FROM `skill_fishing_base_level` WHERE `entry` IN (377, 170);
INSERT INTO `skill_fishing_base_level` (`entry`, `skill`) VALUES
(377, -20),
(170, -20);
