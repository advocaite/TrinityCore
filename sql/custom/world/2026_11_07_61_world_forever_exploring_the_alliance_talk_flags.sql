-- Exploring the Alliance (93963, Skyborne): "Speak with High Tinker Mekkatorque / King Magni Bronzebeard / Tyrande
-- Whisperwind" could not be done (Discord, Russian player 2026-11-07: "can't click to talk to Mekkatorque and Bronzebeard").
-- Cause: the three have only the quest-giver flag (2). With no quest to offer, a click opens nothing, so the talk is never
-- sent and the talk-to objective is never credited. Same fault the Horde side had (Vol'jin, Nazgrel: 2026_10_27_16).
-- Fix: add the gossip flag. A click then always opens their window (their quests stay in it) and credits the objective.
-- These were the only Forever talk-to objectives left on NPCs without the gossip flag (checked against the 10-27 world).
-- Safe to run again.
UPDATE `creature_template` SET `npcflag` = `npcflag` | 1 WHERE `entry` IN (7937, 2784, 7999);
