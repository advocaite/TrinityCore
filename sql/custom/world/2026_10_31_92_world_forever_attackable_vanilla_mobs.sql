-- Vanilla world mobs that players could not attack or click.
-- Their creature_template rows came from the 69497 base data with unit_flags 0x2000000 (uninteractible), 0x100/0x200
-- (immune to players / NPCs) and sometimes 0x40000 (stunned). Every spawn of these is a vanilla spawn (vMaNGOS guid range or
-- vMaNGOS spawn list), they have vanilla loot, no ScriptName, no SmartAI that changes their flags, and in vanilla they are
-- ordinary hostile mobs. Quests blocked by this: Battle of Hillsbrad 541 (Dun Garok), Unfinished Business 6004 (Scarlet
-- Hunters/Medics), Avenge My Village 6548 (Grimtotem Ruffians), Khan Jehn 93128.
-- Only those bits are cleared, and only while the row still has the old value, so the file can run more than once.
UPDATE `creature_template` SET `unit_flags` = 0 WHERE `entry` = 597 AND `unit_flags` = 33554688; -- Bloodscalp Berserker (57 spawns)
UPDATE `creature_template` SET `unit_flags` = 0 WHERE `entry` = 832 AND `unit_flags` = 256; -- Dust Devil (16 spawns)
UPDATE `creature_template` SET `unit_flags` = 0 WHERE `entry` = 1831 AND `unit_flags` = 537133312; -- Scarlet Hunter (29 spawns, quest target)
UPDATE `creature_template` SET `unit_flags` = 0 WHERE `entry` = 1835 AND `unit_flags` = 537133312; -- Scarlet Invoker (29 spawns)
UPDATE `creature_template` SET `unit_flags` = 0 WHERE `entry` = 2344 AND `unit_flags` = 570688256; -- Dun Garok Mountaineer (25 spawns, quest target)
UPDATE `creature_template` SET `unit_flags` = 0 WHERE `entry` = 2345 AND `unit_flags` = 570688256; -- Dun Garok Rifleman (12 spawns, quest target)
UPDATE `creature_template` SET `unit_flags` = 0 WHERE `entry` = 2346 AND `unit_flags` = 570688256; -- Dun Garok Priest (6 spawns, quest target)
UPDATE `creature_template` SET `unit_flags` = 0 WHERE `entry` = 3383 AND `unit_flags` = 256; -- Southsea Cutthroat (22 spawns)
UPDATE `creature_template` SET `unit_flags` = 0 WHERE `entry` = 3725 AND `unit_flags` = 570688256; -- Dark Strand Cultist (10 spawns)
UPDATE `creature_template` SET `unit_flags` = 0 WHERE `entry` = 3727 AND `unit_flags` = 570688256; -- Dark Strand Enforcer (6 spawns)
UPDATE `creature_template` SET `unit_flags` = 0 WHERE `entry` = 3728 AND `unit_flags` = 570688256; -- Dark Strand Adept (10 spawns)
UPDATE `creature_template` SET `unit_flags` = 0 WHERE `entry` = 3730 AND `unit_flags` = 570688256; -- Dark Strand Excavator (6 spawns)
UPDATE `creature_template` SET `unit_flags` = 0 WHERE `entry` = 4006 AND `unit_flags` = 256; -- Deepmoss Webspinner (45 spawns)
UPDATE `creature_template` SET `unit_flags` = 0 WHERE `entry` = 4639 AND `unit_flags` = 33555200; -- Magram Outrunner (13 spawns)
UPDATE `creature_template` SET `unit_flags` = 0 WHERE `entry` = 4640 AND `unit_flags` = 33555200; -- Magram Wrangler (39 spawns)
UPDATE `creature_template` SET `unit_flags` = 0 WHERE `entry` = 4641 AND `unit_flags` = 33555200; -- Magram Windchaser (38 spawns)
UPDATE `creature_template` SET `unit_flags` = 0 WHERE `entry` = 4643 AND `unit_flags` = 33555200; -- Magram Pack Runner (3 spawns)
UPDATE `creature_template` SET `unit_flags` = 0 WHERE `entry` = 4644 AND `unit_flags` = 33555200; -- Magram Marauder (12 spawns)
UPDATE `creature_template` SET `unit_flags` = 0 WHERE `entry` = 4645 AND `unit_flags` = 33555200; -- Magram Mauler (12 spawns)
UPDATE `creature_template` SET `unit_flags` = 0 WHERE `entry` = 4662 AND `unit_flags` = 33555200; -- Magram Bonepaw (6 spawns)
UPDATE `creature_template` SET `unit_flags` = 0 WHERE `entry` = 5254 AND `unit_flags` = 537133824; -- Woodpaw Mystic (92 spawns)
UPDATE `creature_template` SET `unit_flags` = 0 WHERE `entry` = 5419 AND `unit_flags` = 537133824; -- Glasshide Basilisk (76 spawns)
UPDATE `creature_template` SET `unit_flags` = 0 WHERE `entry` = 5601 AND `unit_flags` = 33555200; -- Khan Jehn (1 spawns, quest target)
UPDATE `creature_template` SET `unit_flags` = 0 WHERE `entry` = 8555 AND `unit_flags` = 570688256; -- Crypt Fiend (12 spawns)
UPDATE `creature_template` SET `unit_flags` = 0 WHERE `entry` = 8556 AND `unit_flags` = 570688256; -- Crypt Walker (14 spawns)
UPDATE `creature_template` SET `unit_flags` = 0 WHERE `entry` = 8557 AND `unit_flags` = 570688256; -- Crypt Horror (13 spawns)
UPDATE `creature_template` SET `unit_flags` = 0 WHERE `entry` = 8558 AND `unit_flags` = 570688256; -- Crypt Slayer (36 spawns)
UPDATE `creature_template` SET `unit_flags` = 0 WHERE `entry` = 10605 AND `unit_flags` = 537133312; -- Scarlet Medic (29 spawns, quest target)
UPDATE `creature_template` SET `unit_flags` = 0 WHERE `entry` = 11910 AND `unit_flags` = 33555200; -- Grimtotem Ruffian (50 spawns, quest target)
UPDATE `creature_template` SET `unit_flags` = 0 WHERE `entry` = 12379 AND `unit_flags` = 537133824; -- Unliving Caretaker (10 spawns)
UPDATE `creature_template` SET `unit_flags` = 0 WHERE `entry` = 12380 AND `unit_flags` = 537133824; -- Unliving Resident (22 spawns)
