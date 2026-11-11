-- WoW Forever: City of Dalaran (map 2959), boss values checked against a live run.
-- Data from Efficient Games (https://efficient.games), licensed under CC BY 4.0 (EG Link output for 1.60.1.70338: creature
-- records and one full run's combat log). Changed: only the values below are taken.
--  * Shade of the Archmage 246020: we had guessed it (model 3585, 14x health, undead mage). Live record: model 130061 at
--    scale 1.5, 13x health, 4.5x mana, elemental, difficulty id 325513, type flags 136314984 / 128. Live kill: level 33.
--  * Lyn the Ignored 247032: 12x health (we had 5x; live kill 15696 health at level 31) and level 31 (we had 33).
--  * Arcanic Enigma 246016: level 30 (we had 29).
-- Safe to run again.
UPDATE `creature_template` SET `type` = 4, `unit_class` = 2 WHERE `entry` = 246020;
UPDATE `creature_template_difficulty` SET `HealthModifier` = 13.0, `ManaModifier` = 4.5, `CreatureDifficultyID` = 325513,
    `TypeFlags` = 136314984, `TypeFlags2` = 128 WHERE `Entry` = 246020 AND `DifficultyID` = 0;
DELETE FROM `creature_template_model` WHERE `CreatureID` = 246020;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES
(246020,0,130061,1.5,1.0,0);
UPDATE `creature_template_difficulty` SET `HealthModifier` = 12.0 WHERE `Entry` = 247032 AND `DifficultyID` = 0;
UPDATE `creature_classic_level` SET `level_min` = 31, `level_max` = 31 WHERE `entry` = 247032;
UPDATE `creature_classic_level` SET `level_min` = 30, `level_max` = 30 WHERE `entry` = 246016;
