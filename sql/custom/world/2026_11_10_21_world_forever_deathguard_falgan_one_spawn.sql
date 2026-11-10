-- Classic 1.60 (WoW Forever): Deathguard Falgan (257663, undead paladin Redemption chain, A Lesson in Divinity 94438) stood at Venomweb Vale
-- three times: 25766300 and 25766301 (Forever quest data restore, 15 yards apart) and 46000003 (our Wowhead estimate, between them).
-- He lies dead until a paladin uses the Symbol of Life on him (2026_11_10_20), so the three looked like three dead Falgans. One is kept.
-- PC check 2026-11-10 (CHECK-REDEMPTION-READONLY.sql): 3 spawns near 2400,-875. Safe to run again.
DELETE FROM `creature_addon` WHERE `guid` IN (25766301, 46000003);
DELETE FROM `creature` WHERE `guid` IN (25766301, 46000003) AND `id` = 257663;
