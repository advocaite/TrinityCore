-- WoW Forever live realm (client 70862): fixes from two packet captures of Blizzards realm, 2026-11-07 (The Barrens, Orgrimmar)
-- and 2026-11-10 (a new mage in Coldridge Valley). Only things the captures show clearly and our world data lacks or has wrong.
-- Can run more than once.

-- 1. Kul Tiras Marine and Corporal Adamore (Barrens camp): live faction 3624 (Alliance) and their weapons. Our spawns had no gear.
UPDATE `creature_template` SET `faction` = 3624 WHERE `entry` IN (261368, 262119);
DELETE FROM `creature_equip_template` WHERE `CreatureID` IN (261368, 262119) AND `ID` = 1;
INSERT INTO `creature_equip_template` (`CreatureID`, `ID`, `ItemID1`, `AppearanceModID1`, `ItemVisual1`, `ItemID2`, `AppearanceModID2`, `ItemVisual2`, `ItemID3`, `AppearanceModID3`, `ItemVisual3`, `VerifiedBuild`) VALUES
(261368, 1, 1899, 0, 0, 2053, 0, 0, 0, 0, 0, 70862),
(262119, 1, 1983, 0, 0, 0, 0, 0, 0, 0, 0, 70862);
UPDATE `creature` SET `equipment_id` = 1 WHERE `id` IN (261368, 262119) AND `equipment_id` = 0;

-- 2. Gear the live NPCs carry where we have none: the Kolkar and Horde fighters of the Regthar Deathgate event and five Orgrimmar
--    traders. INSERT IGNORE: a row we already have stays. Spawns that had no gear set get set 1.
INSERT IGNORE INTO `creature_equip_template` (`CreatureID`, `ID`, `ItemID1`, `AppearanceModID1`, `ItemVisual1`, `ItemID2`, `AppearanceModID2`, `ItemVisual2`, `ItemID3`, `AppearanceModID3`, `ItemVisual3`, `VerifiedBuild`) VALUES
(9456, 1, 3433, 0, 0, 0, 0, 0, 0, 0, 0, 70862),
(9457, 1, 12285, 0, 0, 0, 0, 0, 0, 0, 0, 70862),
(9458, 1, 10611, 0, 0, 0, 0, 0, 5856, 0, 0, 70862),
(9523, 1, 5304, 0, 0, 0, 0, 0, 0, 0, 0, 70862),
(9524, 1, 2023, 0, 0, 0, 0, 0, 0, 0, 0, 70862),
(3309, 1, 12852, 0, 0, 12855, 0, 0, 0, 0, 0, 70862),
(3318, 1, 12856, 0, 0, 12859, 0, 0, 0, 0, 0, 70862),
(3319, 1, 0, 0, 0, 1985, 0, 0, 0, 0, 0, 70862),
(3320, 1, 12850, 0, 0, 12745, 0, 0, 0, 0, 0, 70862),
(15116, 1, 12850, 0, 0, 0, 0, 0, 0, 0, 0, 70862);
UPDATE `creature` SET `equipment_id` = 1 WHERE `id` IN (3309, 3318, 3319, 3320, 15116) AND `equipment_id` = 0;

-- 3. Orgrimmar Grunt and Horde Guard: live ranged weapon on their backs (59 and 21 seen, all the same).
UPDATE `creature_equip_template` SET `ItemID1` = 5289, `ItemID2` = 0, `ItemID3` = 5258, `VerifiedBuild` = 70862 WHERE `CreatureID` = 3296 AND `ID` = 1;
UPDATE `creature_equip_template` SET `ItemID1` = 12285, `ItemID2` = 0, `ItemID3` = 2550, `VerifiedBuild` = 70862 WHERE `CreatureID` = 3501 AND `ID` = 1;

-- 4. Models: Regthar Deathgate event NPCs and Sayge get the live models if they have none (INSERT IGNORE);
--    Ragged Young Wolf (705) has a new model on live.
INSERT IGNORE INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`) VALUES
(9456, 0, 4874, 1, 100, 70862),
(9457, 0, 8649, 1, 25, 70862),
(9457, 1, 9575, 1, 25, 70862),
(9457, 2, 9576, 1, 25, 70862),
(9457, 3, 9577, 1, 25, 70862),
(9458, 0, 9578, 1, 25, 70862),
(9458, 1, 8653, 1, 25, 70862),
(9458, 2, 9579, 1, 25, 70862),
(9458, 3, 9580, 1, 25, 70862),
(14822, 0, 491, 1, 100, 70862);
DELETE FROM `creature_template_model` WHERE `CreatureID` = 705;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`) VALUES
(705, 0, 138618, 1, 1, 70862);

-- 5. Idle animations of four Orgrimmar NPCs on live (Lanti gah, Duhng, Zamja, Kor geld).
INSERT INTO `creature_template_addon` (`entry`, `emote`) VALUES (9990, 233), (8306, 173), (3399, 69), (3348, 69) ON DUPLICATE KEY UPDATE `emote` = VALUES(`emote`);

-- 6. Campfire 2700: live it is a cooking fire (spell focus 4, radius 3), ours was an old chest record. One live spawn at the
--    Kul Tiras camp in The Barrens, we had none. guid block 43100000-43100099 is for these sniff spawns.
UPDATE `gameobject_template` SET `type` = 8, `displayId` = 192, `size` = 1, `Data0` = 4, `Data1` = 3, `Data2` = 0, `Data3` = 0, `Data4` = 0, `Data5` = 0, `Data6` = 0, `Data7` = 0, `Data8` = 0, `Data9` = 0, `Data10` = 0, `Data11` = 0, `Data12` = 0, `Data13` = 0, `Data14` = 0, `Data15` = 0, `Data16` = 0, `Data17` = 0, `Data18` = 0, `Data19` = 0, `Data20` = 0, `Data21` = 0, `Data22` = 0, `Data23` = 0, `Data24` = 0, `Data25` = 0, `Data26` = 0, `Data27` = 0, `Data28` = 0, `Data29` = 0, `Data30` = 0, `Data31` = 0, `Data32` = 0, `Data33` = 0, `Data34` = 0, `VerifiedBuild` = 70862 WHERE `entry` = 2700;
DELETE FROM `gameobject` WHERE `guid` BETWEEN 43100000 AND 43100099;
INSERT INTO `gameobject` (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnDifficulties`, `phaseUseFlags`, `PhaseId`, `PhaseGroup`, `terrainSwapMap`, `position_x`, `position_y`, `position_z`, `orientation`, `rotation0`, `rotation1`, `rotation2`, `rotation3`, `spawntimesecs`, `animprogress`, `state`, `ScriptName`, `StringId`, `VerifiedBuild`) VALUES
(43100000, 2700, 1, 0, 0, '0', 0, 0, 0, -1, 548.0799, -1618.066, 91.6956, 0, 0, 0.0783, 0, 0.9969, 120, 255, 1, '', NULL, 70862),
(43100001, 562111, 0, 0, 0, '0', 0, 0, 0, -1, -6519.1997, 599.7465, 403.1787, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100002, 562111, 0, 0, 0, '0', 0, 0, 0, -1, -6421.6548, 803.8924, 397.4496, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100003, 562111, 0, 0, 0, '0', 0, 0, 0, -1, -6236.2378, 620.9288, 389.721, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100004, 656160, 0, 0, 0, '0', 0, 0, 0, -1, -6427.1943, 436.2795, 383.896, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100005, 656160, 0, 0, 0, '0', 0, 0, 0, -1, -6426.6235, 402.4062, 385.8368, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100006, 656160, 0, 0, 0, '0', 0, 0, 0, -1, -6395.8872, 547.5555, 385.1827, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100007, 656160, 0, 0, 0, '0', 0, 0, 0, -1, -6384.3369, 707.1111, 387.6658, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100008, 656160, 0, 0, 0, '0', 0, 0, 0, -1, -6362.5469, 336.3663, 381.5862, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100009, 656160, 0, 0, 0, '0', 0, 0, 0, -1, -6267.3267, 505.9323, 386.9975, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100010, 656160, 0, 0, 0, '0', 0, 0, 0, -1, -6251.6406, 495.8854, 387.7228, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100011, 656160, 0, 0, 0, '0', 0, 0, 0, -1, -6239.6318, 659.5833, 387.2256, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100012, 656160, 0, 0, 0, '0', 0, 0, 0, -1, -6233.8926, 395.7674, 389.8149, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100013, 656160, 0, 0, 0, '0', 0, 0, 0, -1, -6215.6304, 466.684, 388.2902, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100014, 656160, 0, 0, 0, '0', 0, 0, 0, -1, -6194.2275, 423.2361, 389.4691, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100015, 656160, 0, 0, 0, '0', 0, 0, 0, -1, -6186.2676, 663.7465, 386.0542, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100016, 656161, 0, 0, 0, '0', 0, 0, 0, -1, -6522.686, 557.5469, 394.762, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100017, 656161, 0, 0, 0, '0', 0, 0, 0, -1, -6443.002, 613.8403, 387.5386, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100018, 656161, 0, 0, 0, '0', 0, 0, 0, -1, -6425.4409, 527.6042, 385.5395, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100019, 656161, 0, 0, 0, '0', 0, 0, 0, -1, -6318.6997, 745.632, 387.2283, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100020, 656161, 0, 0, 0, '0', 0, 0, 0, -1, -6281.9116, 333.4931, 375.6289, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100021, 656161, 0, 0, 0, '0', 0, 0, 0, -1, -6251.0088, 526.5799, 386.7703, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100022, 656161, 0, 0, 0, '0', 0, 0, 0, -1, -6238.2275, 359.5174, 384.9989, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100023, 656161, 0, 0, 0, '0', 0, 0, 0, -1, -6199.9829, 593.3455, 387.9684, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862);

-- 7. Starter gathering nodes in Dun Morogh (Wilted Peacebloom, Stunted Silverleaf, Poor Copper Vein): 23 live places more than
--    25 yd from any of ours (rows above, from 43100001), added to their node pools. The pools keep their limits, so the number of
--    nodes up at once does not change, they only have more places to appear.
DELETE FROM `pool_members` WHERE `type` = 1 AND `spawnId` BETWEEN 43100000 AND 43100099;
INSERT INTO `pool_members` (`type`, `spawnId`, `poolSpawnId`, `chance`, `description`) VALUES
(1, 43100001, 1400101, 0, 'Poor Copper Vein in the Eastern Kingdoms starting zones'),
(1, 43100002, 1400101, 0, 'Poor Copper Vein in the Eastern Kingdoms starting zones'),
(1, 43100003, 1400101, 0, 'Poor Copper Vein in the Eastern Kingdoms starting zones'),
(1, 43100004, 1400102, 0, 'Wilted Peacebloom in the Eastern Kingdoms starting zones'),
(1, 43100005, 1400102, 0, 'Wilted Peacebloom in the Eastern Kingdoms starting zones'),
(1, 43100006, 1400102, 0, 'Wilted Peacebloom in the Eastern Kingdoms starting zones'),
(1, 43100007, 1400102, 0, 'Wilted Peacebloom in the Eastern Kingdoms starting zones'),
(1, 43100008, 1400102, 0, 'Wilted Peacebloom in the Eastern Kingdoms starting zones'),
(1, 43100009, 1400102, 0, 'Wilted Peacebloom in the Eastern Kingdoms starting zones'),
(1, 43100010, 1400102, 0, 'Wilted Peacebloom in the Eastern Kingdoms starting zones'),
(1, 43100011, 1400102, 0, 'Wilted Peacebloom in the Eastern Kingdoms starting zones'),
(1, 43100012, 1400102, 0, 'Wilted Peacebloom in the Eastern Kingdoms starting zones'),
(1, 43100013, 1400102, 0, 'Wilted Peacebloom in the Eastern Kingdoms starting zones'),
(1, 43100014, 1400102, 0, 'Wilted Peacebloom in the Eastern Kingdoms starting zones'),
(1, 43100015, 1400102, 0, 'Wilted Peacebloom in the Eastern Kingdoms starting zones'),
(1, 43100016, 1400103, 0, 'Stunted Silverleaf in the Eastern Kingdoms starting zones'),
(1, 43100017, 1400103, 0, 'Stunted Silverleaf in the Eastern Kingdoms starting zones'),
(1, 43100018, 1400103, 0, 'Stunted Silverleaf in the Eastern Kingdoms starting zones'),
(1, 43100019, 1400103, 0, 'Stunted Silverleaf in the Eastern Kingdoms starting zones'),
(1, 43100020, 1400103, 0, 'Stunted Silverleaf in the Eastern Kingdoms starting zones'),
(1, 43100021, 1400103, 0, 'Stunted Silverleaf in the Eastern Kingdoms starting zones'),
(1, 43100022, 1400103, 0, 'Stunted Silverleaf in the Eastern Kingdoms starting zones'),
(1, 43100023, 1400103, 0, 'Stunted Silverleaf in the Eastern Kingdoms starting zones');

-- 8. Orgrimmar district and shop signs: live shows their name when the mouse is over them (Data0 = 1).
UPDATE `gameobject_template` SET `Data0` = 1 WHERE `type` = 5 AND `entry` IN (658883, 658884, 658886, 658896, 658897, 658898, 658899, 658900, 658901, 658902, 658903, 658904, 658905, 658906, 658907, 658908, 658909, 658910, 658916, 658941, 658943, 658944, 658945, 658947, 658967, 658979, 659006, 659092, 659097, 659099, 659153, 659155, 659157);

-- 9. Linen Cloth from Rockjaw Trogg and Burly Rockjaw Trogg (Coldridge Valley): live drops it (2 of 9 kills); ours never.
--    Chance 20, about what the sample shows and below the 28-30 of the other Dun Morogh troggs and trolls.
DELETE FROM `creature_loot_template` WHERE `Entry` IN (707, 724) AND `Item` = 2589 AND `ItemType` = 0;
INSERT INTO `creature_loot_template` (`Entry`, `ItemType`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(707, 0, 2589, 20, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-10'),
(724, 0, 2589, 20, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-10');
