-- Teleport: Orgrimmar (3567) and the mage portal's Portal Effect: Orgrimmar (17609) still pointed at the Orgrimmar rebuilt in
-- Cataclysm. On the Classic 1.60 map the old city stands there, so the player arrived under the ground. Back to the vanilla spot
-- in the Valley of Strength (both spells: teleport effect 0, destination from the database). Safe to run again.
DELETE FROM `spell_target_position` WHERE `ID` IN (3567, 17609) AND `EffectIndex` = 0;
INSERT INTO `spell_target_position` (`ID`, `EffectIndex`, `OrderIndex`, `MapID`, `PositionX`, `PositionY`, `PositionZ`, `Orientation`, `VerifiedBuild`) VALUES
(3567, 0, 0, 1, 1676.21, -4315.29, 61.5292, 1.54, 0),
(17609, 0, 0, 1, 1676.21, -4315.29, 61.5292, 1.54, 0);
