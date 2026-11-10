-- Garments of the Light (5624, human priest): Lesser Heal and Power Word: Fortitude on Guard Roberts (12423) landed on the priest
-- instead (by_hoy_, 2026-11-08). His template carried unit_flags 768 (immune to players and to NPCs), and a player cannot cast a
-- helpful spell on a unit that is immune to players, so the client cast it on the priest. The other Garments NPCs (Mountaineer
-- Dolf 12427, Deathguard Kel 12428, Sentinel Shaya 12429) have 0 there; Roberts now matches them. Spawn overrides cleared too.
-- Check in game: target Guard Roberts and cast Lesser Heal (Rank 2): he gets healed, thanks you and the quest counts. Safe to run again.
UPDATE `creature_template` SET `unit_flags` = `unit_flags` & ~768 WHERE `entry` = 12423;
UPDATE `creature` SET `unit_flags` = `unit_flags` & ~768 WHERE `id` = 12423 AND `unit_flags` IS NOT NULL;
