-- Stoneclaw Totem rank 1 (3579) still had a SmartAI script from the world data: "Out of Combat - Cast 5729 on Self".
-- An AIName wins over the core TotemAI, so rank 1 never cast its own spell 5728 (set in
-- 2026_11_06_40_world_forever_shaman_totem_spells.sql) and never drew the enemies to it. Ranks 2 and up (3911, 3912, 3913,
-- 7398, 7399, 15478) have no AIName and work. Same fix as Searing Totem ranks 1 and 2: give it back to TotemAI.
-- Safe to run again.
UPDATE `creature_template` SET `AIName`='' WHERE `entry`=3579 AND `AIName`='SmartAI';
DELETE FROM `smart_scripts` WHERE `source_type`=0 AND `entryorguid`=3579;
