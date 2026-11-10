-- Searing Totem rank 1 does not attack (Discord, Zoryn 2026-11-09). Rank 2 has the same problem.
--
-- Cause: totems that attack are run by the core TotemAI, which casts the totem's spell (creature_template_spell, see
-- 2026_11_06_40_world_forever_shaman_totem_spells.sql) at the nearest enemy in range. But Searing Totem 2523 (rank 1) and
-- 3902 (rank 2) still had AIName 'SmartAI' from the world data, and an AIName wins over TotemAI:
--   2523: "Out of Combat - Cast 3606 on Self" (the totem tried to burn itself, which fails, and never picked an enemy)
--   3902: "In Combat - Cast 6350 on Victim" (a totem never enters combat, so this never ran)
-- Ranks 3 to 6 (3903, 3904, 7400, 7402) have no AIName and work. Checked in the dev DB dump of 2026-11-04.
-- Fix: give both back to TotemAI, like the other ranks. Their spells (3606, 6350) are already set per rank. Safe to run again.
UPDATE `creature_template` SET `AIName`='' WHERE `entry` IN (2523,3902) AND `AIName`='SmartAI';
DELETE FROM `smart_scripts` WHERE `source_type`=0 AND `entryorguid` IN (2523,3902);
