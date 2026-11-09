-- Our own spawns of Forever NPCs could not be clicked: the cursor did not change and nothing opened (Discord, Russian player
-- 2026-11-04: "Eldrun Stormbreaker, the Ironforge shaman trainer, is there but you can't talk to him").
-- Cause: in this core the npcflag and unit_flags columns of `creature` override the template when they are not NULL, and 0 is
-- not NULL. 2026_10_31_70 (Eldrun Stormbreaker 258098, guid 46001000) and 2026_10_30_10 (quest givers and quest NPCs, guids
-- 46000000-46000999) wrote 0 there, so these spawns had no gossip, trainer or quest flag at all, whatever the template said.
-- Fix: NULL, so the spawns use their template flags again (as every other spawn does, and as 2026_11_02_29 and the hire brokers
-- already write).
-- Safe to run again.

UPDATE `creature` SET `npcflag` = NULL, `unit_flags` = NULL, `unit_flags2` = NULL, `unit_flags3` = NULL
WHERE (`guid` BETWEEN 46000000 AND 46000999 OR `guid` = 46001000)
  AND (`npcflag` IS NOT NULL OR `unit_flags` IS NOT NULL OR `unit_flags2` IS NOT NULL OR `unit_flags3` IS NOT NULL);
