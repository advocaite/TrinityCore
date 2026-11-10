-- The Attack! (434, Stormwind, zeroyka on Discord 11-09): Tyrion's Spybot never went to Lescovar and Lescovar never walked to
-- the garden, so nobody could overhear the talk or fight the two. The VMaNGOS import had only carried parts of the event into
-- SmartAI (its waypoint and quest-start scripts were cut short). The whole event is now the C++ port in
-- classic_stormwind_city.cpp (VMaNGOS 2023 rewrite: quest_start_scripts 434, creature_movement_scripts 175406-175408,
-- 777905-777932, 885605, generic_scripts 4340/4341). The walks use the paths already in the database: 810077790
-- (spybot as Priestess Tyriona), 810017540 (Lescovar), 810088560 (spybot coming home). Safe to run again.

-- The old, half-imported SmartAI rows would fight the script: drop them.
DELETE FROM `smart_scripts` WHERE `source_type`=0 AND `entryorguid` IN (7766, 8856, 1754, 1755);
DELETE FROM `smart_scripts` WHERE `source_type`=9 AND `entryorguid` IN (950000018, 950001106, 950001107, 950001108, 950001109);

UPDATE `creature_template` SET `AIName`='', `ScriptName`='classic_npc_tyrion' WHERE `entry`=7766;
UPDATE `creature_template` SET `AIName`='', `ScriptName`='classic_npc_tyrions_spybot' WHERE `entry`=8856;
UPDATE `creature_template` SET `AIName`='', `ScriptName`='classic_npc_lord_gregor_lescovar' WHERE `entry`=1754;
UPDATE `creature_template` SET `AIName`='', `ScriptName`='classic_npc_marzon_the_silent_blade' WHERE `entry`=1755;

-- While the spybot wears Priestess Tyriona's entry nobody can attack it and it attacks nobody (VMaNGOS unit_flags 768).
UPDATE `creature_template` SET `unit_flags` = `unit_flags` | 0x300 WHERE `entry` = 7779; -- UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_IMMUNE_TO_NPC
