-- WoW Forever live realm (client 70862): a new character on Zephras Isle (map 2991), capture 2026-11-10 23:13.
-- Only what our data lacks or has wrong. Can run more than once. New guids 43100200-43100299, new pools 1400112-1400140.

-- 1. Poor Copper Vein on Zephras Isle. Live showed it 17 times at 15 places, our island has none. At 12 of them we spawn a Copper Vein
--    on the exact same spot (live showed Copper Vein only 3 times), so each of those spots becomes a small pool: Poor Copper 80, Copper 20.
--    3 more live places have no node of ours nearby and get a Poor Copper Vein of their own.
--    All these spots sit in one new pool (1400127) with 12 up at once (the same 80% the Copper Vein pool uses).
--    The Copper Vein pool (1400108) loses those 12 spots, so its limit drops from 140 to 128 to keep the same share up.
DELETE FROM `pool_members` WHERE `type` = 1 AND `spawnId` BETWEEN 43100200 AND 43100299;
DELETE FROM `pool_members` WHERE `type` = 2 AND (`spawnId` BETWEEN 1400112 AND 1400140 OR `poolSpawnId` BETWEEN 1400112 AND 1400140);
DELETE FROM `pool_members` WHERE `type` = 1 AND `spawnId` IN (21100408,21100314,21100308,21100311,21100310,21100404,21100401,21100304,21100306,43000304,21100305,21100402);
DELETE FROM `pool_template` WHERE `entry` BETWEEN 1400112 AND 1400140;
DELETE FROM `gameobject` WHERE `guid` BETWEEN 43100200 AND 43100299;
INSERT INTO `gameobject` (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnDifficulties`, `phaseUseFlags`, `PhaseId`, `PhaseGroup`, `terrainSwapMap`, `position_x`, `position_y`, `position_z`, `orientation`, `rotation0`, `rotation1`, `rotation2`, `rotation3`, `spawntimesecs`, `animprogress`, `state`, `ScriptName`, `StringId`, `VerifiedBuild`) VALUES
(43100200, 562111, 2991, 0, 0, '0', 0, 0, 0, -1, 3686.6885, 2193.125, 955.2718, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100201, 562111, 2991, 0, 0, '0', 0, 0, 0, -1, 3720.8247, 2289.177, 949.9666, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100202, 562111, 2991, 0, 0, '0', 0, 0, 0, -1, 3829.0574, 1677.566, 948.5051, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100203, 562111, 2991, 0, 0, '0', 0, 0, 0, -1, 3871.0244, 2173.448, 997.7352, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100204, 562111, 2991, 0, 0, '0', 0, 0, 0, -1, 3916.7751, 1993.2135, 1001.7614, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100205, 562111, 2991, 0, 0, '0', 0, 0, 0, -1, 3923.6936, 1975.4479, 992.8662, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100206, 562111, 2991, 0, 0, '0', 0, 0, 0, -1, 3967.3723, 1577.151, 958.9626, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100207, 562111, 2991, 0, 0, '0', 0, 0, 0, -1, 4005.7908, 1775.2812, 966.4636, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100208, 562111, 2991, 0, 0, '0', 0, 0, 0, -1, 4028.0999, 1750.0295, 960.0431, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100209, 562111, 2991, 0, 0, '0', 0, 0, 0, -1, 4088.2891, 2266.481, 989.1829, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100210, 562111, 2991, 0, 0, '0', 0, 0, 0, -1, 4192.0571, 1787.6406, 962.6824, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100211, 562111, 2991, 0, 0, '0', 0, 0, 0, -1, 4304.0645, 1643.1841, 940.5302, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100212, 562111, 2991, 0, 0, '0', 0, 0, 0, -1, 3870.0698, 1830.3021, 976.9814, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100213, 562111, 2991, 0, 0, '0', 0, 0, 0, -1, 3927.5461, 2100.6321, 1006.9219, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100214, 562111, 2991, 0, 0, '0', 0, 0, 0, -1, 4051.3706, 2186.7102, 1006.2923, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100250, 1617, 2991, 0, 0, '0', 0, 0, 0, -1, 4096.3057, 1694.2848, 957.3889, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100251, 1618, 2991, 0, 0, '0', 0, 0, 0, -1, 3757.4504, 2099.6301, 958.0462, 6.2504, 0, 0, -0.0164, 0.9999, 120, 255, 1, '', NULL, 70862),
(43100252, 1619, 2991, 0, 0, '0', 0, 0, 0, -1, 3819.2327, 2001.2673, 971.6848, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100253, 1619, 2991, 0, 0, '0', 0, 0, 0, -1, 3877.7744, 1830.7327, 974.015, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100254, 1619, 2991, 0, 0, '0', 0, 0, 0, -1, 3942.1182, 1958.9774, 989.8224, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100255, 1731, 2991, 0, 0, '0', 0, 0, 0, -1, 3655.9163, 1652.5944, 844.7329, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100256, 1731, 2991, 0, 0, '0', 0, 0, 0, -1, 3776.5149, 1533.4827, 957.1055, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862);
INSERT INTO `pool_template` (`entry`, `max_limit`, `description`) VALUES
(1400112, 1, 'Zephyris copper spot 1'),
(1400113, 1, 'Zephyris copper spot 2'),
(1400114, 1, 'Zephyris copper spot 3'),
(1400115, 1, 'Zephyris copper spot 4'),
(1400116, 1, 'Zephyris copper spot 5'),
(1400117, 1, 'Zephyris copper spot 6'),
(1400118, 1, 'Zephyris copper spot 7'),
(1400119, 1, 'Zephyris copper spot 8'),
(1400120, 1, 'Zephyris copper spot 9'),
(1400121, 1, 'Zephyris copper spot 10'),
(1400122, 1, 'Zephyris copper spot 11'),
(1400123, 1, 'Zephyris copper spot 12'),
(1400124, 1, 'Zephyris copper spot 13'),
(1400125, 1, 'Zephyris copper spot 14'),
(1400126, 1, 'Zephyris copper spot 15'),
(1400127, 12, 'Copper or Poor Copper spots on Zephyris');
INSERT INTO `pool_members` (`type`, `spawnId`, `poolSpawnId`, `chance`, `description`) VALUES
(1, 43100200, 1400112, 80, 'Poor Copper Vein on Zephyris (live)'),
(1, 21100408, 1400112, 20, 'Copper Vein on Zephyris'),
(1, 43100201, 1400113, 80, 'Poor Copper Vein on Zephyris (live)'),
(1, 21100314, 1400113, 20, 'Copper Vein on Zephyris'),
(1, 43100202, 1400114, 80, 'Poor Copper Vein on Zephyris (live)'),
(1, 21100308, 1400114, 20, 'Copper Vein on Zephyris'),
(1, 43100203, 1400115, 80, 'Poor Copper Vein on Zephyris (live)'),
(1, 21100311, 1400115, 20, 'Copper Vein on Zephyris'),
(1, 43100204, 1400116, 80, 'Poor Copper Vein on Zephyris (live)'),
(1, 21100310, 1400116, 20, 'Copper Vein on Zephyris'),
(1, 43100205, 1400117, 80, 'Poor Copper Vein on Zephyris (live)'),
(1, 21100404, 1400117, 20, 'Copper Vein on Zephyris'),
(1, 43100206, 1400118, 80, 'Poor Copper Vein on Zephyris (live)'),
(1, 21100401, 1400118, 20, 'Copper Vein on Zephyris'),
(1, 43100207, 1400119, 80, 'Poor Copper Vein on Zephyris (live)'),
(1, 21100304, 1400119, 20, 'Copper Vein on Zephyris'),
(1, 43100208, 1400120, 80, 'Poor Copper Vein on Zephyris (live)'),
(1, 21100306, 1400120, 20, 'Copper Vein on Zephyris'),
(1, 43100209, 1400121, 80, 'Poor Copper Vein on Zephyris (live)'),
(1, 43000304, 1400121, 20, 'Copper Vein on Zephyris'),
(1, 43100210, 1400122, 80, 'Poor Copper Vein on Zephyris (live)'),
(1, 21100305, 1400122, 20, 'Copper Vein on Zephyris'),
(1, 43100211, 1400123, 80, 'Poor Copper Vein on Zephyris (live)'),
(1, 21100402, 1400123, 20, 'Copper Vein on Zephyris'),
(1, 43100212, 1400124, 0, 'Poor Copper Vein on Zephyris (live)'),
(1, 43100213, 1400125, 0, 'Poor Copper Vein on Zephyris (live)'),
(1, 43100214, 1400126, 0, 'Poor Copper Vein on Zephyris (live)'),
(2, 1400112, 1400127, 0, 'Zephyris copper spot 1'),
(2, 1400113, 1400127, 0, 'Zephyris copper spot 2'),
(2, 1400114, 1400127, 0, 'Zephyris copper spot 3'),
(2, 1400115, 1400127, 0, 'Zephyris copper spot 4'),
(2, 1400116, 1400127, 0, 'Zephyris copper spot 5'),
(2, 1400117, 1400127, 0, 'Zephyris copper spot 6'),
(2, 1400118, 1400127, 0, 'Zephyris copper spot 7'),
(2, 1400119, 1400127, 0, 'Zephyris copper spot 8'),
(2, 1400120, 1400127, 0, 'Zephyris copper spot 9'),
(2, 1400121, 1400127, 0, 'Zephyris copper spot 10'),
(2, 1400122, 1400127, 0, 'Zephyris copper spot 11'),
(2, 1400123, 1400127, 0, 'Zephyris copper spot 12'),
(2, 1400124, 1400127, 0, 'Zephyris copper spot 13'),
(2, 1400125, 1400127, 0, 'Zephyris copper spot 14'),
(2, 1400126, 1400127, 0, 'Zephyris copper spot 15'),
(1, 43100250, 1400104, 0, 'Silverleaf on Zephyris'),
(1, 43100251, 1400105, 0, 'Peacebloom on Zephyris'),
(1, 43100252, 1400106, 0, 'Earthroot on Zephyris'),
(1, 43100253, 1400106, 0, 'Earthroot on Zephyris'),
(1, 43100254, 1400106, 0, 'Earthroot on Zephyris'),
(1, 43100255, 1400108, 0, 'Copper Vein on Zephyris'),
(1, 43100256, 1400108, 0, 'Copper Vein on Zephyris');
UPDATE `pool_template` SET `max_limit` = 128 WHERE `entry` = 1400108;

-- 2. 7 more live places for the Zephras herb and copper pools, each more than 25 yd from any node we have (guids from 43100250).
--    They are in the INSERT above. The pools keep their limits, so the nodes just have more places to appear.

-- 3. Dented Chest: live had one 46 yd from our nearest of 7. It gets its own spawn (guid 43100290), 120 s respawn like ours.
INSERT INTO `gameobject` (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnDifficulties`, `phaseUseFlags`, `PhaseId`, `PhaseGroup`, `terrainSwapMap`, `position_x`, `position_y`, `position_z`, `orientation`, `rotation0`, `rotation1`, `rotation2`, `rotation3`, `spawntimesecs`, `animprogress`, `state`, `ScriptName`, `StringId`, `VerifiedBuild`) VALUES
(43100290, 626718, 2991, 0, 0, '0', 0, 0, 0, -1, 3733.8743, 2207.2036, 957.9366, 4.4383, 0, 0, -0.7971, 0.6039, 120, 255, 1, '', NULL, 70862);

-- 4. Belandiel Farflight (254360, Trade Supplies on Zephras): her live list has 18 items. We lacked 4536, 277113, 277114 and 277115
--    (the last three our other Zephras traders already sell), and we had 2321, 2605, 2692 and 6530, which live does not sell. Live order kept.
DELETE FROM `npc_vendor` WHERE `entry` = 254360;
INSERT INTO `npc_vendor` (`entry`, `slot`, `item`, `maxcount`, `incrtime`, `ExtendedCost`, `type`, `BonusListIDs`, `PlayerConditionID`, `IgnoreFiltering`, `VerifiedBuild`) VALUES
(254360, 0, 7005, 0, 0, 0, 1, NULL, 0, 0, 70862),
(254360, 1, 2901, 0, 0, 0, 1, NULL, 0, 0, 70862),
(254360, 2, 5956, 0, 0, 0, 1, NULL, 0, 0, 70862),
(254360, 3, 6256, 0, 0, 0, 1, NULL, 0, 0, 70862),
(254360, 4, 6217, 0, 0, 0, 1, NULL, 0, 0, 70862),
(254360, 5, 2678, 0, 0, 0, 1, NULL, 0, 0, 70862),
(254360, 6, 2320, 0, 0, 0, 1, NULL, 0, 0, 70862),
(254360, 7, 2880, 0, 0, 0, 1, NULL, 0, 0, 70862),
(254360, 8, 3371, 0, 0, 0, 1, NULL, 0, 0, 70862),
(254360, 9, 4289, 0, 0, 0, 1, NULL, 0, 0, 70862),
(254360, 10, 6529, 0, 0, 0, 1, NULL, 0, 0, 70862),
(254360, 11, 2604, 0, 0, 0, 1, NULL, 0, 0, 70862),
(254360, 12, 2324, 0, 0, 0, 1, NULL, 0, 0, 70862),
(254360, 13, 6260, 0, 0, 0, 1, NULL, 0, 0, 70862),
(254360, 14, 4536, 0, 0, 0, 1, NULL, 0, 0, 70862),
(254360, 15, 277115, 0, 0, 0, 1, NULL, 0, 0, 70862),
(254360, 16, 277114, 0, 0, 0, 1, NULL, 0, 0, 70862),
(254360, 17, 277113, 0, 0, 0, 1, NULL, 0, 0, 70862);

-- 5. Dorii Brightwhisper (251379), the Zephras mage trainer: her menu 41122 had no "I'd like training!" line, so mages could not
--    open her training list. Live has it (option 136807). Her list stays our trainer 5, which has the same 6 spells as live trainer 1235.
DELETE FROM `gossip_menu_option` WHERE `MenuID` = 41122 AND `GossipOptionID` = 136807;
INSERT INTO `gossip_menu_option` (`MenuID`, `GossipOptionID`, `OptionID`, `OptionNpc`, `OptionText`, `OptionBroadcastTextID`, `Language`, `Flags`, `ActionMenuID`, `ActionPoiID`, `GossipNpcOptionID`, `BoxCoded`, `BoxMoney`, `BoxText`, `BoxBroadcastTextID`, `SpellID`, `OverrideIconID`, `VerifiedBuild`) VALUES
(41122, 136807, 0, 3, 'I''d like training!', 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 70862);
UPDATE `creature_trainer` SET `MenuID` = 41122, `OptionID` = 0 WHERE `CreatureID` = 251379 AND `TrainerID` = 5;

-- 6. Mage training costs (trainer list 1, used by every mage trainer): 140 of its spells cost something else on live (Shenaan Spellwind,
--    Zephras). Some of ours were far off, for example Pyroblast rank 2 (12505) cost 200 copper with us and 4300 on live. Only the price
--    changes here, the required levels stay ours.
UPDATE `trainer_spell` SET `MoneyCost` = 2100 WHERE `TrainerId` = 1 AND `SpellId` = 10; -- was 2000
UPDATE `trainer_spell` SET `MoneyCost` = 4800 WHERE `TrainerId` = 1 AND `SpellId` = 120; -- was 5000
UPDATE `trainer_spell` SET `MoneyCost` = 300 WHERE `TrainerId` = 1 AND `SpellId` = 122; -- was 400
UPDATE `trainer_spell` SET `MoneyCost` = 2100 WHERE `TrainerId` = 1 AND `SpellId` = 543; -- was 2000
UPDATE `trainer_spell` SET `MoneyCost` = 5500 WHERE `TrainerId` = 1 AND `SpellId` = 759; -- was 7000
UPDATE `trainer_spell` SET `MoneyCost` = 4800 WHERE `TrainerId` = 1 AND `SpellId` = 865; -- was 5000
UPDATE `trainer_spell` SET `MoneyCost` = 3300 WHERE `TrainerId` = 1 AND `SpellId` = 990; -- was 3000
UPDATE `trainer_spell` SET `MoneyCost` = 5500 WHERE `TrainerId` = 1 AND `SpellId` = 1461; -- was 7000
UPDATE `trainer_spell` SET `MoneyCost` = 2100 WHERE `TrainerId` = 1 AND `SpellId` = 1463; -- was 2000
UPDATE `trainer_spell` SET `MoneyCost` = 2100 WHERE `TrainerId` = 1 AND `SpellId` = 1953; -- was 2000
UPDATE `trainer_spell` SET `MoneyCost` = 1700 WHERE `TrainerId` = 1 AND `SpellId` = 2120; -- was 1500
UPDATE `trainer_spell` SET `MoneyCost` = 4300 WHERE `TrainerId` = 1 AND `SpellId` = 2121; -- was 4000
UPDATE `trainer_spell` SET `MoneyCost` = 3300 WHERE `TrainerId` = 1 AND `SpellId` = 2138; -- was 3000
UPDATE `trainer_spell` SET `MoneyCost` = 4300 WHERE `TrainerId` = 1 AND `SpellId` = 2139; -- was 4000
UPDATE `trainer_spell` SET `MoneyCost` = 3300 WHERE `TrainerId` = 1 AND `SpellId` = 2948; -- was 3000
UPDATE `trainer_spell` SET `MoneyCost` = 12000 WHERE `TrainerId` = 1 AND `SpellId` = 3552; -- was 14000
UPDATE `trainer_spell` SET `MoneyCost` = 1700 WHERE `TrainerId` = 1 AND `SpellId` = 5144; -- was 1500
UPDATE `trainer_spell` SET `MoneyCost` = 4300 WHERE `TrainerId` = 1 AND `SpellId` = 5145; -- was 4000
UPDATE `trainer_spell` SET `MoneyCost` = 300 WHERE `TrainerId` = 1 AND `SpellId` = 5505; -- was 400
UPDATE `trainer_spell` SET `MoneyCost` = 2100 WHERE `TrainerId` = 1 AND `SpellId` = 5506; -- was 2000
UPDATE `trainer_spell` SET `MoneyCost` = 9000 WHERE `TrainerId` = 1 AND `SpellId` = 6117; -- was 12000
UPDATE `trainer_spell` SET `MoneyCost` = 6200 WHERE `TrainerId` = 1 AND `SpellId` = 6127; -- was 8000
UPDATE `trainer_spell` SET `MoneyCost` = 8000 WHERE `TrainerId` = 1 AND `SpellId` = 6129; -- was 10000
UPDATE `trainer_spell` SET `MoneyCost` = 13000 WHERE `TrainerId` = 1 AND `SpellId` = 6131; -- was 15000
UPDATE `trainer_spell` SET `MoneyCost` = 5500 WHERE `TrainerId` = 1 AND `SpellId` = 6141; -- was 7000
UPDATE `trainer_spell` SET `MoneyCost` = 3300 WHERE `TrainerId` = 1 AND `SpellId` = 6143; -- was 3000
UPDATE `trainer_spell` SET `MoneyCost` = 300 WHERE `TrainerId` = 1 AND `SpellId` = 7300; -- was 400
UPDATE `trainer_spell` SET `MoneyCost` = 2100 WHERE `TrainerId` = 1 AND `SpellId` = 7301; -- was 2000
UPDATE `trainer_spell` SET `MoneyCost` = 6200 WHERE `TrainerId` = 1 AND `SpellId` = 7302; -- was 8000
UPDATE `trainer_spell` SET `MoneyCost` = 13000 WHERE `TrainerId` = 1 AND `SpellId` = 7320; -- was 15000
UPDATE `trainer_spell` SET `MoneyCost` = 2100 WHERE `TrainerId` = 1 AND `SpellId` = 7322; -- was 2000
UPDATE `trainer_spell` SET `MoneyCost` = 4300 WHERE `TrainerId` = 1 AND `SpellId` = 8400; -- was 4000
UPDATE `trainer_spell` SET `MoneyCost` = 6200 WHERE `TrainerId` = 1 AND `SpellId` = 8401; -- was 8000
UPDATE `trainer_spell` SET `MoneyCost` = 11000 WHERE `TrainerId` = 1 AND `SpellId` = 8402; -- was 13000
UPDATE `trainer_spell` SET `MoneyCost` = 4800 WHERE `TrainerId` = 1 AND `SpellId` = 8406; -- was 5000
UPDATE `trainer_spell` SET `MoneyCost` = 8000 WHERE `TrainerId` = 1 AND `SpellId` = 8407; -- was 10000
UPDATE `trainer_spell` SET `MoneyCost` = 12000 WHERE `TrainerId` = 1 AND `SpellId` = 8408; -- was 14000
UPDATE `trainer_spell` SET `MoneyCost` = 6200 WHERE `TrainerId` = 1 AND `SpellId` = 8412; -- was 8000
UPDATE `trainer_spell` SET `MoneyCost` = 12000 WHERE `TrainerId` = 1 AND `SpellId` = 8413; -- was 14000
UPDATE `trainer_spell` SET `MoneyCost` = 8000 WHERE `TrainerId` = 1 AND `SpellId` = 8416; -- was 10000
UPDATE `trainer_spell` SET `MoneyCost` = 13000 WHERE `TrainerId` = 1 AND `SpellId` = 8417; -- was 15000
UPDATE `trainer_spell` SET `MoneyCost` = 8000 WHERE `TrainerId` = 1 AND `SpellId` = 8422; -- was 10000
UPDATE `trainer_spell` SET `MoneyCost` = 13000 WHERE `TrainerId` = 1 AND `SpellId` = 8423; -- was 15000
UPDATE `trainer_spell` SET `MoneyCost` = 11000 WHERE `TrainerId` = 1 AND `SpellId` = 8427; -- was 13000
UPDATE `trainer_spell` SET `MoneyCost` = 3300 WHERE `TrainerId` = 1 AND `SpellId` = 8437; -- was 3000
UPDATE `trainer_spell` SET `MoneyCost` = 6200 WHERE `TrainerId` = 1 AND `SpellId` = 8438; -- was 8000
UPDATE `trainer_spell` SET `MoneyCost` = 12000 WHERE `TrainerId` = 1 AND `SpellId` = 8439; -- was 14000
UPDATE `trainer_spell` SET `MoneyCost` = 5500 WHERE `TrainerId` = 1 AND `SpellId` = 8444; -- was 7000
UPDATE `trainer_spell` SET `MoneyCost` = 9000 WHERE `TrainerId` = 1 AND `SpellId` = 8445; -- was 12000
UPDATE `trainer_spell` SET `MoneyCost` = 13000 WHERE `TrainerId` = 1 AND `SpellId` = 8446; -- was 15000
UPDATE `trainer_spell` SET `MoneyCost` = 4300 WHERE `TrainerId` = 1 AND `SpellId` = 8450; -- was 4000
UPDATE `trainer_spell` SET `MoneyCost` = 11000 WHERE `TrainerId` = 1 AND `SpellId` = 8451; -- was 13000
UPDATE `trainer_spell` SET `MoneyCost` = 6200 WHERE `TrainerId` = 1 AND `SpellId` = 8455; -- was 8000
UPDATE `trainer_spell` SET `MoneyCost` = 6200 WHERE `TrainerId` = 1 AND `SpellId` = 8457; -- was 8000
UPDATE `trainer_spell` SET `MoneyCost` = 13000 WHERE `TrainerId` = 1 AND `SpellId` = 8458; -- was 15000
UPDATE `trainer_spell` SET `MoneyCost` = 8000 WHERE `TrainerId` = 1 AND `SpellId` = 8461; -- was 10000
UPDATE `trainer_spell` SET `MoneyCost` = 14000 WHERE `TrainerId` = 1 AND `SpellId` = 8462; -- was 18000
UPDATE `trainer_spell` SET `MoneyCost` = 9000 WHERE `TrainerId` = 1 AND `SpellId` = 8492; -- was 12000
UPDATE `trainer_spell` SET `MoneyCost` = 5500 WHERE `TrainerId` = 1 AND `SpellId` = 8494; -- was 7000
UPDATE `trainer_spell` SET `MoneyCost` = 11000 WHERE `TrainerId` = 1 AND `SpellId` = 8495; -- was 13000
UPDATE `trainer_spell` SET `MoneyCost` = 21000 WHERE `TrainerId` = 1 AND `SpellId` = 10053; -- was 28000
UPDATE `trainer_spell` SET `MoneyCost` = 29000 WHERE `TrainerId` = 1 AND `SpellId` = 10054; -- was 40000
UPDATE `trainer_spell` SET `MoneyCost` = 13000 WHERE `TrainerId` = 1 AND `SpellId` = 10138; -- was 15000
UPDATE `trainer_spell` SET `MoneyCost` = 23000 WHERE `TrainerId` = 1 AND `SpellId` = 10139; -- was 32000
UPDATE `trainer_spell` SET `MoneyCost` = 14000 WHERE `TrainerId` = 1 AND `SpellId` = 10144; -- was 18000
UPDATE `trainer_spell` SET `MoneyCost` = 24000 WHERE `TrainerId` = 1 AND `SpellId` = 10145; -- was 35000
UPDATE `trainer_spell` SET `MoneyCost` = 14000 WHERE `TrainerId` = 1 AND `SpellId` = 10148; -- was 18000
UPDATE `trainer_spell` SET `MoneyCost` = 21000 WHERE `TrainerId` = 1 AND `SpellId` = 10149; -- was 28000
UPDATE `trainer_spell` SET `MoneyCost` = 26000 WHERE `TrainerId` = 1 AND `SpellId` = 10150; -- was 36000
UPDATE `trainer_spell` SET `MoneyCost` = 30000 WHERE `TrainerId` = 1 AND `SpellId` = 10151; -- was 42000
UPDATE `trainer_spell` SET `MoneyCost` = 14000 WHERE `TrainerId` = 1 AND `SpellId` = 10156; -- was 18000
UPDATE `trainer_spell` SET `MoneyCost` = 28000 WHERE `TrainerId` = 1 AND `SpellId` = 10157; -- was 38000
UPDATE `trainer_spell` SET `MoneyCost` = 14000 WHERE `TrainerId` = 1 AND `SpellId` = 10159; -- was 18000
UPDATE `trainer_spell` SET `MoneyCost` = 23000 WHERE `TrainerId` = 1 AND `SpellId` = 10160; -- was 32000
UPDATE `trainer_spell` SET `MoneyCost` = 29000 WHERE `TrainerId` = 1 AND `SpellId` = 10161; -- was 40000
UPDATE `trainer_spell` SET `MoneyCost` = 14000 WHERE `TrainerId` = 1 AND `SpellId` = 10169; -- was 18000
UPDATE `trainer_spell` SET `MoneyCost` = 26000 WHERE `TrainerId` = 1 AND `SpellId` = 10170; -- was 36000
UPDATE `trainer_spell` SET `MoneyCost` = 21000 WHERE `TrainerId` = 1 AND `SpellId` = 10173; -- was 28000
UPDATE `trainer_spell` SET `MoneyCost` = 30000 WHERE `TrainerId` = 1 AND `SpellId` = 10174; -- was 42000
UPDATE `trainer_spell` SET `MoneyCost` = 24000 WHERE `TrainerId` = 1 AND `SpellId` = 10177; -- was 35000
UPDATE `trainer_spell` SET `MoneyCost` = 15000 WHERE `TrainerId` = 1 AND `SpellId` = 10179; -- was 23000
UPDATE `trainer_spell` SET `MoneyCost` = 23000 WHERE `TrainerId` = 1 AND `SpellId` = 10180; -- was 32000
UPDATE `trainer_spell` SET `MoneyCost` = 28000 WHERE `TrainerId` = 1 AND `SpellId` = 10181; -- was 38000
UPDATE `trainer_spell` SET `MoneyCost` = 15000 WHERE `TrainerId` = 1 AND `SpellId` = 10185; -- was 23000
UPDATE `trainer_spell` SET `MoneyCost` = 24000 WHERE `TrainerId` = 1 AND `SpellId` = 10186; -- was 35000
UPDATE `trainer_spell` SET `MoneyCost` = 30000 WHERE `TrainerId` = 1 AND `SpellId` = 10187; -- was 42000
UPDATE `trainer_spell` SET `MoneyCost` = 15000 WHERE `TrainerId` = 1 AND `SpellId` = 10191; -- was 23000
UPDATE `trainer_spell` SET `MoneyCost` = 24000 WHERE `TrainerId` = 1 AND `SpellId` = 10192; -- was 35000
UPDATE `trainer_spell` SET `MoneyCost` = 30000 WHERE `TrainerId` = 1 AND `SpellId` = 10193; -- was 42000
UPDATE `trainer_spell` SET `MoneyCost` = 20000 WHERE `TrainerId` = 1 AND `SpellId` = 10197; -- was 26000
UPDATE `trainer_spell` SET `MoneyCost` = 26000 WHERE `TrainerId` = 1 AND `SpellId` = 10199; -- was 36000
UPDATE `trainer_spell` SET `MoneyCost` = 20000 WHERE `TrainerId` = 1 AND `SpellId` = 10201; -- was 26000
UPDATE `trainer_spell` SET `MoneyCost` = 26000 WHERE `TrainerId` = 1 AND `SpellId` = 10202; -- was 36000
UPDATE `trainer_spell` SET `MoneyCost` = 20000 WHERE `TrainerId` = 1 AND `SpellId` = 10205; -- was 26000
UPDATE `trainer_spell` SET `MoneyCost` = 24000 WHERE `TrainerId` = 1 AND `SpellId` = 10206; -- was 35000
UPDATE `trainer_spell` SET `MoneyCost` = 29000 WHERE `TrainerId` = 1 AND `SpellId` = 10207; -- was 40000
UPDATE `trainer_spell` SET `MoneyCost` = 21000 WHERE `TrainerId` = 1 AND `SpellId` = 10211; -- was 28000
UPDATE `trainer_spell` SET `MoneyCost` = 28000 WHERE `TrainerId` = 1 AND `SpellId` = 10212; -- was 38000
UPDATE `trainer_spell` SET `MoneyCost` = 21000 WHERE `TrainerId` = 1 AND `SpellId` = 10215; -- was 28000
UPDATE `trainer_spell` SET `MoneyCost` = 28000 WHERE `TrainerId` = 1 AND `SpellId` = 10216; -- was 38000
UPDATE `trainer_spell` SET `MoneyCost` = 23000 WHERE `TrainerId` = 1 AND `SpellId` = 10219; -- was 32000
UPDATE `trainer_spell` SET `MoneyCost` = 30000 WHERE `TrainerId` = 1 AND `SpellId` = 10220; -- was 42000
UPDATE `trainer_spell` SET `MoneyCost` = 23000 WHERE `TrainerId` = 1 AND `SpellId` = 10223; -- was 32000
UPDATE `trainer_spell` SET `MoneyCost` = 30000 WHERE `TrainerId` = 1 AND `SpellId` = 10225; -- was 42000
UPDATE `trainer_spell` SET `MoneyCost` = 26000 WHERE `TrainerId` = 1 AND `SpellId` = 10230; -- was 36000
UPDATE `trainer_spell` SET `MoneyCost` = 2100 WHERE `TrainerId` = 1 AND `SpellId` = 12051; -- was 2000
UPDATE `trainer_spell` SET `MoneyCost` = 4300 WHERE `TrainerId` = 1 AND `SpellId` = 12505; -- was 200
UPDATE `trainer_spell` SET `MoneyCost` = 6200 WHERE `TrainerId` = 1 AND `SpellId` = 12522; -- was 400
UPDATE `trainer_spell` SET `MoneyCost` = 11000 WHERE `TrainerId` = 1 AND `SpellId` = 12523; -- was 650
UPDATE `trainer_spell` SET `MoneyCost` = 14000 WHERE `TrainerId` = 1 AND `SpellId` = 12524; -- was 900
UPDATE `trainer_spell` SET `MoneyCost` = 21000 WHERE `TrainerId` = 1 AND `SpellId` = 12525; -- was 1400
UPDATE `trainer_spell` SET `MoneyCost` = 26000 WHERE `TrainerId` = 1 AND `SpellId` = 12526; -- was 1800
UPDATE `trainer_spell` SET `MoneyCost` = 2100 WHERE `TrainerId` = 1 AND `SpellId` = 12824; -- was 2000
UPDATE `trainer_spell` SET `MoneyCost` = 13000 WHERE `TrainerId` = 1 AND `SpellId` = 12825; -- was 15000
UPDATE `trainer_spell` SET `MoneyCost` = 30000 WHERE `TrainerId` = 1 AND `SpellId` = 12826; -- was 42000
UPDATE `trainer_spell` SET `MoneyCost` = 11000 WHERE `TrainerId` = 1 AND `SpellId` = 13018; -- was 650
UPDATE `trainer_spell` SET `MoneyCost` = 15000 WHERE `TrainerId` = 1 AND `SpellId` = 13019; -- was 1150
UPDATE `trainer_spell` SET `MoneyCost` = 24000 WHERE `TrainerId` = 1 AND `SpellId` = 13020; -- was 1750
UPDATE `trainer_spell` SET `MoneyCost` = 30000 WHERE `TrainerId` = 1 AND `SpellId` = 13021; -- was 2100
UPDATE `trainer_spell` SET `MoneyCost` = 20000 WHERE `TrainerId` = 1 AND `SpellId` = 13031; -- was 1300
UPDATE `trainer_spell` SET `MoneyCost` = 24000 WHERE `TrainerId` = 1 AND `SpellId` = 13032; -- was 1750
UPDATE `trainer_spell` SET `MoneyCost` = 29000 WHERE `TrainerId` = 1 AND `SpellId` = 13033; -- was 2000
UPDATE `trainer_spell` SET `MoneyCost` = 30000 WHERE `TrainerId` = 1 AND `SpellId` = 18809; -- was 2100
UPDATE `trainer_spell` SET `MoneyCost` = 21000 WHERE `TrainerId` = 1 AND `SpellId` = 22782; -- was 28000
UPDATE `trainer_spell` SET `MoneyCost` = 29000 WHERE `TrainerId` = 1 AND `SpellId` = 22783; -- was 40000
UPDATE `trainer_spell` SET `MoneyCost` = 5500 WHERE `TrainerId` = 1 AND `SpellId` = 400640; -- was 3000
UPDATE `trainer_spell` SET `MoneyCost` = 13000 WHERE `TrainerId` = 1 AND `SpellId` = 401502; -- was 15000
UPDATE `trainer_spell` SET `MoneyCost` = 23000 WHERE `TrainerId` = 1 AND `SpellId` = 1237312; -- was 32000
UPDATE `trainer_spell` SET `MoneyCost` = 30000 WHERE `TrainerId` = 1 AND `SpellId` = 1237313; -- was 42000
UPDATE `trainer_spell` SET `MoneyCost` = 6200 WHERE `TrainerId` = 1 AND `SpellId` = 1239696; -- was 8000
UPDATE `trainer_spell` SET `MoneyCost` = 13000 WHERE `TrainerId` = 1 AND `SpellId` = 1239697; -- was 15000
UPDATE `trainer_spell` SET `MoneyCost` = 23000 WHERE `TrainerId` = 1 AND `SpellId` = 1239699; -- was 32000
UPDATE `trainer_spell` SET `MoneyCost` = 30000 WHERE `TrainerId` = 1 AND `SpellId` = 1239700; -- was 42000
UPDATE `trainer_spell` SET `MoneyCost` = 9000 WHERE `TrainerId` = 1 AND `SpellId` = 1240044; -- was 14500
UPDATE `trainer_spell` SET `MoneyCost` = 14000 WHERE `TrainerId` = 1 AND `SpellId` = 1240045; -- was 18000
UPDATE `trainer_spell` SET `MoneyCost` = 21000 WHERE `TrainerId` = 1 AND `SpellId` = 1240046; -- was 32000
UPDATE `trainer_spell` SET `MoneyCost` = 28000 WHERE `TrainerId` = 1 AND `SpellId` = 1240047; -- was 40000
UPDATE `trainer_spell` SET `MoneyCost` = 11000 WHERE `TrainerId` = 1 AND `SpellId` = 1326080; -- was 11578
UPDATE `trainer_spell` SET `MoneyCost` = 22000 WHERE `TrainerId` = 1 AND `SpellId` = 1326081; -- was 23157
UPDATE `trainer_spell` SET `MoneyCost` = 30000 WHERE `TrainerId` = 1 AND `SpellId` = 1326082; -- was 32631

-- 7. Live renamed Ley Line to Leyline on Zephras: the quest Reading the Leylines (92597), its objective and three objects.
UPDATE `gameobject_template` SET `name` = 'Leyline' WHERE `entry` IN (602735, 613248);
UPDATE `gameobject_template` SET `name` = 'Leyline Trap (Quest)' WHERE `entry` = 602739;
UPDATE `quest_template` SET `LogTitle` = 'Reading the Leylines', `LogDescription` = 'Use your Read Leyline racial ability at the leyline in Thendal Grove.', `QuestDescription` = 'Here in the elemental plane, we derive much of our arcane strength from leylines that still run through the ground under our feet. Of late, these leylines have begun to pierce the surface, giving us more direct and powerful access to these latent arcane energies.' WHERE `ID` = 92597;
UPDATE `quest_objectives` SET `Description` = 'Use your Read Leyline ability near the Thendal Grove Leyline' WHERE `ID` = 465190;

-- 8. Quest texts we had empty: the turn-in text of 96638 and the "not done yet" texts of 92463 and 92474.
INSERT INTO `quest_offer_reward` (`ID`, `Emote1`, `Emote2`, `Emote3`, `Emote4`, `EmoteDelay1`, `EmoteDelay2`, `EmoteDelay3`, `EmoteDelay4`, `RewardText`, `VerifiedBuild`) VALUES
(96638, 1, 0, 0, 0, 0, 0, 0, 0, 'Well met!', 70862) ON DUPLICATE KEY UPDATE `RewardText` = VALUES(`RewardText`);
INSERT INTO `quest_request_items` (`ID`, `EmoteOnComplete`, `EmoteOnIncomplete`, `EmoteOnCompleteDelay`, `EmoteOnIncompleteDelay`, `CompletionText`, `VerifiedBuild`) VALUES
(92463, 1, 1, 0, 0, 'Cirrusflies aren''t to be underestimated. They might not seem dangerous, but you have to watch out for the bigger ones. The bigger they are, the meaner!', 70862),
(92474, 1, 1, 0, 0, 'Using your ability to walk on air can get you out of--and, well... into--all kinds of trouble!', 70862) ON DUPLICATE KEY UPDATE `CompletionText` = VALUES(`CompletionText`);

-- 9. Weapons: three Zephras NPCs had a broken second weapon number in our data (889192448, 1761607680) or a missing off-hand.
UPDATE `creature_equip_template` SET `ItemID1` = 0, `ItemID2` = 18485, `ItemID3` = 0 WHERE `CreatureID` = 251965 AND `ID` = 1; -- Fevrath Skyhammer
UPDATE `creature_equip_template` SET `ItemID1` = 2179, `ItemID2` = 2179, `ItemID3` = 0 WHERE `CreatureID` = 251964 AND `ID` = 1; -- Blademaster Ren
UPDATE `creature_equip_template` SET `ItemID1` = 1897, `ItemID2` = 1897, `ItemID3` = 0 WHERE `CreatureID` = 257554 AND `ID` = 1; -- Halaan Hawk-Eye

-- 10. Idle animations live shows and we lack: Aetheen of the Gales, Myriaal Mistwake, Valreaa Valewind, Hanaa Nightwind, Al'Aketh Ambusher, Rathiril Sunlance.
INSERT INTO `creature_template_addon` (`entry`, `emote`) VALUES
(251366, 1047),
(263113, 1134),
(257551, 376),
(252095, 376),
(251451, 333),
(251903, 1037) ON DUPLICATE KEY UPDATE `emote` = VALUES(`emote`);

-- 11. Grey and common drops live gave that our Zephras mobs never drop (vanilla level 1-5 items). Small samples, so the chance
--     is a low guess: 3 for an item seen once, 10 for one seen twice. Rows we already have stay as they are.
--     The loot table has no unique key, so the rows of this section are removed first to keep a second run from doubling them.
DELETE FROM `creature_loot_template` WHERE `Comment` LIKE 'Forever live drop 2026-11-10 (%';
INSERT INTO `creature_loot_template` (`Entry`, `ItemType`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(250926, 0, 1372, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-10 (Scrawny Ursera, 1 of 15 kills)'),
(250926, 0, 1374, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-10 (Scrawny Ursera, 1 of 15 kills)'),
(250926, 0, 1380, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-10 (Scrawny Ursera, 1 of 15 kills)'),
(250926, 0, 2650, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-10 (Scrawny Ursera, 1 of 15 kills)'),
(250926, 0, 2652, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-10 (Scrawny Ursera, 1 of 15 kills)'),
(250926, 0, 2653, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-10 (Scrawny Ursera, 1 of 15 kills)'),
(250926, 0, 4496, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-10 (Scrawny Ursera, 1 of 15 kills)'),
(250937, 0, 1368, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-10 (Ursera Scavenger, 1 of 9 kills)'),
(250937, 0, 2653, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-10 (Ursera Scavenger, 1 of 9 kills)'),
(250937, 0, 5572, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-10 (Ursera Scavenger, 1 of 9 kills)'),
(251143, 0, 2652, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-10 (Roiling Winds, 1 of 7 kills)'),
(251145, 0, 2652, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-10 (Al''Aketh Brute, 1 of 6 kills)'),
(251160, 0, 1369, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-10 (Al''Aketh Convert, 1 of 8 kills)'),
(251160, 0, 1377, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-10 (Al''Aketh Convert, 1 of 8 kills)'),
(251160, 0, 4540, 10, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-10 (Al''Aketh Convert, 2 of 8 kills)'),
(251169, 0, 2654, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-10 (Pesky Cirrusfly, 1 of 8 kills)'),
(251448, 0, 3365, 3, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-10 (Al''Aketh Neophyte, 1 of 4 kills)');
