-- Whispering Horror (260396, Forever elite at the Shadowvale camp, drops Whispering Horror Residue 268812 for quest 95328).
-- It had no creature_classic_level row (the sniff import cleared it and saw no level), so it fell back to level 1.
-- Level 13: INFERRED from the player report (elite; camp mobs 260431/259433/259434 are 8-12, quest 95328 is level 10).
DELETE FROM `creature_classic_level` WHERE `entry` = 260396;
INSERT INTO `creature_classic_level` (`entry`,`level_min`,`level_max`) VALUES (260396,13,13);

-- Two of its four map-pin spawns took a floor high above the camp; the sniffed camp mobs next to them stand at z 108.
UPDATE `creature` SET `position_z` = 108.2 WHERE `guid` = 46000004 AND `id` = 260396;
UPDATE `creature` SET `position_z` = 108.0 WHERE `guid` = 46000006 AND `id` = 260396;
