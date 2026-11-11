-- WoW Forever live realm (client 70862): a second capture of a new character in Coldridge Valley, 2026-11-10 19:35.
-- Only what the 2026_11_10_60 file does not have yet. Can run more than once.

-- 1. 9 more live places for the Dun Morogh starter node pools, each more than 25 yd from any node we have (guids from 43100100).
--    The pools keep their limits: same number of nodes up at once, more places for them to appear.
DELETE FROM `pool_members` WHERE `type` = 1 AND `spawnId` BETWEEN 43100100 AND 43100199;
DELETE FROM `gameobject` WHERE `guid` BETWEEN 43100100 AND 43100199;
INSERT INTO `gameobject` (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnDifficulties`, `phaseUseFlags`, `PhaseId`, `PhaseGroup`, `terrainSwapMap`, `position_x`, `position_y`, `position_z`, `orientation`, `rotation0`, `rotation1`, `rotation2`, `rotation3`, `spawntimesecs`, `animprogress`, `state`, `ScriptName`, `StringId`, `VerifiedBuild`) VALUES
(43100100, 562111, 0, 0, 0, '0', 0, 0, 0, -1, -6575.7446, 401.9149, 387.14, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100101, 562111, 0, 0, 0, '0', 0, 0, 0, -1, -6568.2832, 353.1979, 390.1033, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100102, 562111, 0, 0, 0, '0', 0, 0, 0, -1, -6538.8384, 443.5243, 386.4126, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100103, 562111, 0, 0, 0, '0', 0, 0, 0, -1, -6535.6582, 295.9236, 382.6689, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100104, 562111, 0, 0, 0, '0', 0, 0, 0, -1, -6518.8662, 642.3611, 401.091, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100105, 562111, 0, 0, 0, '0', 0, 0, 0, -1, -6465.9009, 382.7344, 381.4627, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100106, 656160, 0, 0, 0, '0', 0, 0, 0, -1, -6483.146, 477.3594, 388.5758, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100107, 656160, 0, 0, 0, '0', 0, 0, 0, -1, -6318.6997, 745.632, 387.2283, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862),
(43100108, 656161, 0, 0, 0, '0', 0, 0, 0, -1, -6194.2275, 423.2361, 389.4691, 0, 0, 0, 0, 1, 120, 255, 1, '', NULL, 70862);
INSERT INTO `pool_members` (`type`, `spawnId`, `poolSpawnId`, `chance`, `description`) VALUES
(1, 43100100, 1400101, 0, 'Poor Copper Vein in the Eastern Kingdoms starting zones'),
(1, 43100101, 1400101, 0, 'Poor Copper Vein in the Eastern Kingdoms starting zones'),
(1, 43100102, 1400101, 0, 'Poor Copper Vein in the Eastern Kingdoms starting zones'),
(1, 43100103, 1400101, 0, 'Poor Copper Vein in the Eastern Kingdoms starting zones'),
(1, 43100104, 1400101, 0, 'Poor Copper Vein in the Eastern Kingdoms starting zones'),
(1, 43100105, 1400101, 0, 'Poor Copper Vein in the Eastern Kingdoms starting zones'),
(1, 43100106, 1400102, 0, 'Wilted Peacebloom in the Eastern Kingdoms starting zones'),
(1, 43100107, 1400102, 0, 'Wilted Peacebloom in the Eastern Kingdoms starting zones'),
(1, 43100108, 1400103, 0, 'Stunted Silverleaf in the Eastern Kingdoms starting zones');

-- 2. Linen Cloth from Frostmane Troll Whelp (Coldridge Valley): live dropped it on 3 of 8 kills, ours never. Chance 30, as the
--    other Dun Morogh Frostmane trolls have in our data.
DELETE FROM `creature_loot_template` WHERE `Entry` = 706 AND `Item` = 2589 AND `ItemType` = 0;
INSERT INTO `creature_loot_template` (`Entry`, `ItemType`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(706, 0, 2589, 30, 0, 1, 0, 1, 1, 'Forever live drop 2026-11-10');

-- 3. Frostmane Troll Whelp says one more aggro line on live, added to its aggro group (0) next to the three we have.
DELETE FROM `creature_text` WHERE `CreatureID` = 706 AND `GroupID` = 0 AND `ID` = 3;
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `Sound`, `SoundPlayType`, `BroadcastTextId`, `TextRange`, `comment`)
SELECT 706, 0, 3, 'My weapon be thirsty!', `Type`, `Language`, `Probability`, `Emote`, `Duration`, `Sound`, `SoundPlayType`, 0, `TextRange`, 'Forever live 2026-11-10'
FROM `creature_text` WHERE `CreatureID` = 706 AND `GroupID` = 0 AND `ID` = 0;

-- 4. Mini Diablo (pet) has the title Lord of Terror on live.
UPDATE `creature_template` SET `subname` = 'Lord of Terror' WHERE `entry` = 272328;
