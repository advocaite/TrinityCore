-- Dalaran: Skyborne Portal to Dalaran 654168 in Stormwind (spell 1278903 -> teleport 1278902) put the player at z 110.75. That spot
-- was a guess (Dalaran side not sniffed), copied from the base of the Skyborne Portal to Stormwind 631299 (z 110.7456), and it is
-- just under the floor there: the sniffed creatures standing on the same platform are at z 110.829 (259084 at 448.96 439.35,
-- 270459 at 444.69 455.46, 276170 at 419.81 459.34). So the player loaded in under the floor and fell below Dalaran to their death
-- (player report 2026-11-08). Same spot, now half a yard above the floor, so the player lands on it.
-- Safe to run again.
DELETE FROM `spell_target_position` WHERE `ID` = 1278902 AND `EffectIndex` = 0;
INSERT INTO `spell_target_position` (`ID`,`EffectIndex`,`OrderIndex`,`MapID`,`PositionX`,`PositionY`,`PositionZ`,`Orientation`,`VerifiedBuild`) VALUES
(1278902,0,0,0,455.13,444.87,111.35,1.10,0);
