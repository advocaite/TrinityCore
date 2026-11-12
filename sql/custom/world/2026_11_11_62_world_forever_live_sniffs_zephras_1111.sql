-- WoW Forever live realm (client 70888), three English captures on Zephras Isle (map 2991), 11-11:
--   dump_1.60.1.70888_2026-11-11_10-08-52.pkt, ..._20-44-07.pkt, ..._22-09-42.pkt (Shen'dar Village, Shen'dar Highlands, Camping 101).
-- Only what our data lacks or has wrong. Every row below is "live capture"; rows that replace one of our guesses
-- (VerifiedBuild 0) say "replaces guess". Chances marked INFERRED are our estimate from small samples. Safe to run more than once.
-- New gameobject guids 43100400-43100449, creature guid 43100400 (reserved to 43100499).

-- 1. Herb and copper places on Zephras. Live showed Silverleaf at 23 places, Peacebloom at 21 places, Earthroot at 6 places, Copper Vein at 24 places; we already had a node within 25 yd of most of them.
--    The 29 places more than 25 yd from any node of the same kind we have join the island pools (10 Silverleaf, 3 Peacebloom, 3 Earthroot, 13 Copper Vein).
--    The pools keep their limits, so the nodes just have more places to appear. 120 s respawn like ours.
DELETE FROM `pool_members` WHERE `type` = 1 AND `spawnId` BETWEEN 43100400 AND 43100449;
DELETE FROM `gameobject` WHERE `guid` BETWEEN 43100400 AND 43100449;
INSERT INTO `gameobject` (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnDifficulties`, `phaseUseFlags`, `PhaseId`, `PhaseGroup`, `terrainSwapMap`, `position_x`, `position_y`, `position_z`, `orientation`, `rotation0`, `rotation1`, `rotation2`, `rotation3`, `spawntimesecs`, `animprogress`, `state`, `ScriptName`, `StringId`, `VerifiedBuild`) VALUES
(43100400, 1617, 2991, 0, 0, '0', 0, 0, 0, -1, 2950.2605, 1469.7223, 768.0096, 0.0006, 0, 0, 0.0003, 1, 120, 255, 1, '', NULL, 70888),
(43100401, 1617, 2991, 0, 0, '0', 0, 0, 0, -1, 2984.665, 2067.0815, 786.6659, 0.0006, 0, 0, 0.0003, 1, 120, 255, 1, '', NULL, 70888),
(43100402, 1617, 2991, 0, 0, '0', 0, 0, 0, -1, 3043.8281, 2192.8665, 787.5467, 0.0006, 0, 0, 0.0003, 1, 120, 255, 1, '', NULL, 70888),
(43100403, 1617, 2991, 0, 0, '0', 0, 0, 0, -1, 3060.1597, 2337.9602, 778.0018, 0.0006, 0, 0, 0.0003, 1, 120, 255, 1, '', NULL, 70888),
(43100404, 1617, 2991, 0, 0, '0', 0, 0, 0, -1, 3224.5886, 2071.9827, 842.389, 0.0006, 0, 0, 0.0003, 1, 120, 255, 1, '', NULL, 70888),
(43100405, 1617, 2991, 0, 0, '0', 0, 0, 0, -1, 3229.7864, 2081.6301, 842.4898, 6.258, 0, 0, -0.0126, 0.9999, 120, 255, 1, '', NULL, 70888),
(43100406, 1617, 2991, 0, 0, '0', 0, 0, 0, -1, 3462.217, 1646.9202, 853.7901, 6.258, 0, 0, -0.0126, 0.9999, 120, 255, 1, '', NULL, 70888),
(43100407, 1617, 2991, 0, 0, '0', 0, 0, 0, -1, 3555.5859, 1896.5747, 910.6656, 6.258, 0, 0, -0.0126, 0.9999, 120, 255, 1, '', NULL, 70888),
(43100408, 1617, 2991, 0, 0, '0', 0, 0, 0, -1, 3566.1624, 1705.3142, 857.6981, 6.258, 0, 0, -0.0126, 0.9999, 120, 255, 1, '', NULL, 70888),
(43100409, 1617, 2991, 0, 0, '0', 0, 0, 0, -1, 3608.1458, 1993.7396, 938.2888, 6.258, 0, 0, -0.0126, 0.9999, 120, 255, 1, '', NULL, 70888),
(43100410, 1618, 2991, 0, 0, '0', 0, 0, 0, -1, 2705.9324, 1820.7135, 770.0123, 0.045, 0, 0, 0.0225, 0.9997, 120, 255, 1, '', NULL, 70888),
(43100411, 1618, 2991, 0, 0, '0', 0, 0, 0, -1, 3064.8743, 2002.3993, 796.3118, 0.045, 0, 0, 0.0225, 0.9997, 120, 255, 1, '', NULL, 70888),
(43100412, 1618, 2991, 0, 0, '0', 0, 0, 0, -1, 3594.2847, 1711.3438, 862.2529, 0.0522, 0, 0, 0.0261, 0.9997, 120, 255, 1, '', NULL, 70888),
(43100413, 1619, 2991, 0, 0, '0', 0, 0, 0, -1, 3161.2422, 2155.9097, 828.5264, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70888),
(43100414, 1619, 2991, 0, 0, '0', 0, 0, 0, -1, 3615.9426, 1966.3507, 932.4678, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70888),
(43100415, 1619, 2991, 0, 0, '0', 0, 0, 0, -1, 3659.6165, 1828.658, 909.5229, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70888),
(43100416, 1731, 2991, 0, 0, '0', 0, 0, 0, -1, 2903.5364, 1818.947, 754.3757, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70888),
(43100417, 1731, 2991, 0, 0, '0', 0, 0, 0, -1, 2985.9053, 1620.1989, 778.8594, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70888),
(43100418, 1731, 2991, 0, 0, '0', 0, 0, 0, -1, 3040.3542, 2199.7483, 790.544, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70888),
(43100419, 1731, 2991, 0, 0, '0', 0, 0, 0, -1, 3043.2639, 1896.9185, 794.8217, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70888),
(43100420, 1731, 2991, 0, 0, '0', 0, 0, 0, -1, 3096.3386, 1729.2118, 754.1707, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70888),
(43100421, 1731, 2991, 0, 0, '0', 0, 0, 0, -1, 3116.9817, 1956.1805, 802.703, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70888),
(43100422, 1731, 2991, 0, 0, '0', 0, 0, 0, -1, 3133.7075, 2329.323, 858.3364, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70888),
(43100423, 1731, 2991, 0, 0, '0', 0, 0, 0, -1, 3395.5461, 1875.8837, 867.9847, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70888),
(43100424, 1731, 2991, 0, 0, '0', 0, 0, 0, -1, 3423.697, 1418.4618, 754.4106, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70888),
(43100425, 1731, 2991, 0, 0, '0', 0, 0, 0, -1, 3449.4175, 2078.7795, 890.4072, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70888),
(43100426, 1731, 2991, 0, 0, '0', 0, 0, 0, -1, 3463.436, 1572.6022, 856.9347, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70888),
(43100427, 1731, 2991, 0, 0, '0', 0, 0, 0, -1, 3642.9905, 1736.4896, 868.289, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70888),
(43100428, 1731, 2991, 0, 0, '0', 0, 0, 0, -1, 3650.9458, 1523.5432, 906.304, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70888);
INSERT INTO `pool_members` (`type`, `spawnId`, `poolSpawnId`, `chance`, `description`) VALUES
(1, 43100400, 1400104, 0, 'Silverleaf on Zephyris (live)'),
(1, 43100401, 1400104, 0, 'Silverleaf on Zephyris (live)'),
(1, 43100402, 1400104, 0, 'Silverleaf on Zephyris (live)'),
(1, 43100403, 1400104, 0, 'Silverleaf on Zephyris (live)'),
(1, 43100404, 1400104, 0, 'Silverleaf on Zephyris (live)'),
(1, 43100405, 1400104, 0, 'Silverleaf on Zephyris (live)'),
(1, 43100406, 1400104, 0, 'Silverleaf on Zephyris (live)'),
(1, 43100407, 1400104, 0, 'Silverleaf on Zephyris (live)'),
(1, 43100408, 1400104, 0, 'Silverleaf on Zephyris (live)'),
(1, 43100409, 1400104, 0, 'Silverleaf on Zephyris (live)'),
(1, 43100410, 1400105, 0, 'Peacebloom on Zephyris (live)'),
(1, 43100411, 1400105, 0, 'Peacebloom on Zephyris (live)'),
(1, 43100412, 1400105, 0, 'Peacebloom on Zephyris (live)'),
(1, 43100413, 1400106, 0, 'Earthroot on Zephyris (live)'),
(1, 43100414, 1400106, 0, 'Earthroot on Zephyris (live)'),
(1, 43100415, 1400106, 0, 'Earthroot on Zephyris (live)'),
(1, 43100416, 1400108, 0, 'Copper Vein on Zephyris (live)'),
(1, 43100417, 1400108, 0, 'Copper Vein on Zephyris (live)'),
(1, 43100418, 1400108, 0, 'Copper Vein on Zephyris (live)'),
(1, 43100419, 1400108, 0, 'Copper Vein on Zephyris (live)'),
(1, 43100420, 1400108, 0, 'Copper Vein on Zephyris (live)'),
(1, 43100421, 1400108, 0, 'Copper Vein on Zephyris (live)'),
(1, 43100422, 1400108, 0, 'Copper Vein on Zephyris (live)'),
(1, 43100423, 1400108, 0, 'Copper Vein on Zephyris (live)'),
(1, 43100424, 1400108, 0, 'Copper Vein on Zephyris (live)'),
(1, 43100425, 1400108, 0, 'Copper Vein on Zephyris (live)'),
(1, 43100426, 1400108, 0, 'Copper Vein on Zephyris (live)'),
(1, 43100427, 1400108, 0, 'Copper Vein on Zephyris (live)'),
(1, 43100428, 1400108, 0, 'Copper Vein on Zephyris (live)');

-- 2. Windshaper Novice Seer (257532, Shen'dar Highlands, the mobs of A Magical Affront 94413): live had one standing 62 yd from our nearest
--    of 14, seen once. It gets its own spawn (creature guid 43100400) with the same settings as our other ones.
DELETE FROM `creature` WHERE `guid` = 43100400;
INSERT INTO `creature` (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnDifficulties`, `phaseUseFlags`, `PhaseId`, `PhaseGroup`, `terrainSwapMap`, `modelid`, `equipment_id`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`, `currentwaypoint`, `curHealthPct`, `MovementType`, `npcflag`, `unit_flags`, `unit_flags2`, `unit_flags3`, `ScriptName`, `StringId`, `VerifiedBuild`) VALUES
(43100400, 257532, 2991, 0, 0, '0', 0, 0, 0, -1, 0, 1, 3267.762, 1975.024, 846.0414, 5.6643, 120, 7, 0, 100, 1, NULL, NULL, NULL, NULL, '', NULL, 70888);

-- 3. Halassa Fernbreeze (257021, Herbalist in Shen'dar Village): on live she takes in Camping 101: Herbalism (97968) and has
--    npcflag 83 (gossip, quest, trainer, profession trainer). Ours had only Syriel Nightrain as ender and flag 17. Syriel stays.
UPDATE `creature_template` SET `npcflag` = 83 WHERE `entry` = 257021;
DELETE FROM `creature_questender` WHERE `id` = 257021 AND `quest` = 97968;
INSERT INTO `creature_questender` (`id`, `quest`, `VerifiedBuild`) VALUES (257021, 97968, 70888);

-- 4. Camping 101: Enchanting (98286, Alliance). Replaces guess: our 2026_10_27_09 gave it objective 9828600 on criteria tree 231651
--    with VerifiedBuild 0. Live confirms the tree and gives the real objective id 477268, the reward spell 1230643 and the race mask
--    6130900294268439629 (the same one its twin 97973 has). The rest of the quest matched live.
DELETE FROM `quest_objectives` WHERE `QuestID` = 98286;
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES
(477268,98286,14,0,0,231651,1,0,32,0,0,0,1,'Raise your enchanting skill to 20',70888);
UPDATE `quest_template` SET `AllowableRaces` = 6130900294268439629, `RewardDisplaySpell1` = 1230643, `VerifiedBuild` = 70888 WHERE `ID` = 98286;

-- 5. A Magical Affront (94413): live now writes "leyline" in its text too (the Ley Line -> Leyline rename of 2026_11_11_10).
UPDATE `quest_template` SET `QuestDescription` = 'The recent emergence of the leylines on the surface of Zephras has been profound for our order. We have more direct and tangible access to the energies that permeate this place and it has increased our understanding of said magic greatly.' WHERE `ID` = 94413;

-- 6. Turn-in texts we did not have: 93461 (Rathiril), 96101 The Great Outdoors and 96646 (Raan Wildwind). The capture holds no
--    emote for them, so none is set. Rows we already have with a text stay as they are.
INSERT INTO `quest_offer_reward` (`ID`, `Emote1`, `Emote2`, `Emote3`, `Emote4`, `EmoteDelay1`, `EmoteDelay2`, `EmoteDelay3`, `EmoteDelay4`, `RewardText`, `VerifiedBuild`) VALUES
(93461, 0, 0, 0, 0, 0, 0, 0, 0, 'Checked in with Rathiril then? Great. Lots of tension between the High Order and the Windshapers these days, but they keep things civil here in Shen''dar. Well, at least in the village anyway. What happens out in the wilds stays in the wilds.\n\nNow that that''s done, you look like someone who might be looking for something to do with their hands. I''ve got a few things that need doing, if you are game for a bit of work?', 70888),
(96101, 0, 0, 0, 0, 0, 0, 0, 0, 'So now that we''ve learned the basics, why don''t you go and learn a tradeskill from the trainers in the village nearby. They will also teach you how to create a camp fixture suitable to your new profession.', 70888),
(96646, 0, 0, 0, 0, 0, 0, 0, 0, 'With this, you''ll be able to set up a camp anywhere out in the wilderness. What you''ve learned is just the basics, so you won''t be able to place too many furnishings around it.\n\nRaise your cooking skill and you might just be able to learn to make even bigger fires.\n\nYou can also learn the basics of furnishing your campsites from other tradeskill trainers if you''re savvy in their skills.', 70888)
ON DUPLICATE KEY UPDATE `RewardText` = IF(`RewardText` IS NULL OR `RewardText` = '', VALUES(`RewardText`), `RewardText`);

-- 7. Camp chair (612275): live Data1 (chair height) is 1, ours 0.
UPDATE `gameobject_template` SET `Data1` = 1 WHERE `entry` = 612275;

-- 8. Al'Aketh Ambusher (251451): one more combat line live shows that we lack. Like the other lines of this NPC it is not
--    tied to any event yet.
DELETE FROM `creature_text` WHERE `CreatureID` = 251451 AND `GroupID` = 3;
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `Sound`, `SoundPlayType`, `BroadcastTextId`, `TextRange`, `comment`) VALUES
(251451, 3, 0, 'For the glory of the windlord!', 12, 0, 100, 0, 0, 0, 0, 0, 0, 'Al''Aketh Ambusher (live capture 70888)');

-- 9. Drops live gave that our Zephras mobs never drop (mostly vanilla greys and whites, plus two Forever items from the bandits).
--    Small samples, so the chance is INFERRED: 3 for an item seen once, 10 for one seen twice. Rows we already have stay as they are.
DELETE FROM `creature_loot_template` WHERE `Comment` LIKE 'Forever live drop 2026-11-11 (%';
INSERT INTO `creature_loot_template` (`Entry`, `ItemType`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(250868, 0, 2138, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-11 (Vuldren, 1 of 5 kills)'),
(250868, 0, 2672, 10, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-11 (Vuldren, 2 of 5 kills)'),
(251245, 0, 2210, 10, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-11 (Prideclaw, 2 of 15 kills)'),
(251245, 0, 2649, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-11 (Prideclaw, 1 of 15 kills)'),
(251245, 0, 2651, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-11 (Prideclaw, 1 of 15 kills)'),
(251245, 0, 2654, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-11 (Prideclaw, 1 of 15 kills)'),
(251261, 0, 1411, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-11 (Hippogryph Matriarch, 1 of 1 kills)'),
(251261, 0, 4775, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-11 (Hippogryph Matriarch, 1 of 1 kills)'),
(251284, 0, 1419, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-11 (Hippogryph Protector, 1 of 6 kills)'),
(251291, 0, 1433, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-11 (Hippogryph Youth, 1 of 7 kills)'),
(251661, 0, 2211, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-11 (Galestrider, 1 of 13 kills)'),
(251918, 0, 766, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-11 (Highlands Bandit, 1 of 19 kills)'),
(251918, 0, 1418, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-11 (Highlands Bandit, 1 of 19 kills)'),
(251918, 0, 252030, 3, 0, 1, 0, 2, 2, 'Forever live drop 2026-11-11 (Highlands Bandit, 1 of 19 kills)'),
(251918, 0, 267474, 10, 0, 1, 0, 2, 2, 'Forever live drop 2026-11-11 (Highlands Bandit, 2 of 19 kills)'),
(257532, 0, 118, 10, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-11 (Windshaper Novice Seer, 2 of 6 kills)'),
(257532, 0, 1374, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-11 (Windshaper Novice Seer, 1 of 6 kills)'),
(257532, 0, 255663, 10, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-11 (Windshaper Novice Seer, 2 of 6 kills)');

-- 10. Sania Silverstream (251904) and Missionary Jasaan (257065), the Al'Aketh missionaries of Falaath Village: on live they
--     have the Al'Aketh faction 3578 (ours 35), seen 7 and 2 times. Live still gave and took 92528, 92529 and 93036 through them,
--     because they carry "interact while hostile" (unit_flags2 0x4000) and cannot be attacked (unit_flags 768). Ours has both flags.
UPDATE `creature_template` SET `faction` = 3578 WHERE `entry` IN (251904, 257065);
