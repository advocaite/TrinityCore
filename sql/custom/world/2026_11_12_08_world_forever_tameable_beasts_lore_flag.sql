-- Beast Lore says "Cannot Be Tamed" on tameable beasts (fox4039, 0.1.35).
-- 2026_11_11_90 gave bit 0x08000000 to the 319 beasts that the live player caches list. Every tameable beast in those caches
-- (211 of 211) has the bit, so the tameable beasts the caches do not list get it too: every DifficultyID 0 entry with the
-- tameable bit 0x00000001. INFERRED from the cache sample, not checked against a live answer for each of these entries.
-- Nothing else changes. Clients keep creature data in Cache\WDB: delete that folder before checking in game. Safe to run again.
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` | 0x08000000
WHERE `DifficultyID` = 0 AND (`TypeFlags` & 0x00000001) <> 0 AND (`TypeFlags` & 0x08000000) = 0;
