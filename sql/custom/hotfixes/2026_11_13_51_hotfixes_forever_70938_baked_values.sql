-- WoW Forever client 1.60.1.70938: values Blizzard built into the 70938 game tables that our older live-push rows still overrode.
-- Compared row by row: 70888 tables, 70938 tables (chris's PC extract, 2026-11-13) and every row of our hotfix files. Every other
-- changed record we hold already has the 70938 value (all class and spell changes of 70938 came as live pushes first).
-- Mounting allowed (MountFlags 13) in Magram Front, Outcast Hideaway, Bristleback Retreat, Evenshade's Overlook, Forlorn Gardens
-- and Valley of Bones (2026_11_10_80 had 0 or 1, which stops mounting there); holiday 161 calendar filter; player condition 168663.
-- The rows and their hotfix_data entries already exist; only the values change (every VerifiedBuild row of each ID). Safe to run again.
UPDATE `area_table` SET `MountFlags`=13 WHERE `ID` IN (2657,16675,16676,16677,16678,16742);
UPDATE `holidays` SET `CalendarFilterType`=3 WHERE `ID`=161;
UPDATE `player_condition` SET `ContentTuningID`=6102, `TraitNodeEntryLogic`=65536 WHERE `ID`=168663;
