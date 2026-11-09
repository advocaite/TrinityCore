-- Azeroth Commerce Authority, Alliance side: Daniel Stitchsong (Leatherworker), Mivin Shadowweave (Tailor) and Nina Surefire
-- (Alchemist) sold their recipes for silver because no sell list of theirs was known (2026_11_03_40). Their Horde counterparts
-- Pawani, Jim'bek and Apothecary Durelle sell the very same items (82, 68 and 29 of them, every one matched) for Merchant's
-- Favor. The Alliance vendors now take the same Favor cost per item (Discord 2026-11-09: recipes bought without Favor).
-- Safe to run again.
UPDATE `npc_vendor` a JOIN `npc_vendor` h ON h.`item` = a.`item` AND h.`type` = a.`type` AND h.`entry` = 248201
SET a.`ExtendedCost` = h.`ExtendedCost` WHERE a.`entry` = 256734;
UPDATE `npc_vendor` a JOIN `npc_vendor` h ON h.`item` = a.`item` AND h.`type` = a.`type` AND h.`entry` = 248202
SET a.`ExtendedCost` = h.`ExtendedCost` WHERE a.`entry` = 256735;
UPDATE `npc_vendor` a JOIN `npc_vendor` h ON h.`item` = a.`item` AND h.`type` = a.`type` AND h.`entry` = 248196
SET a.`ExtendedCost` = h.`ExtendedCost` WHERE a.`entry` = 256729;
