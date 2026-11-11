-- Shen'dralas (zone 16651, the new part of Desolace with Valley of Bones and Magram Front) had no graveyard link, so a death there
-- sent the player to the default graveyard far away. Link it to the two Desolace graveyards that already exist and have a
-- Spirit Healer: Mannoroc Coven (1423, inside the zone's map area) and Ethel Rethor (31). The server picks the nearest one.
-- Safe to run again.
DELETE FROM `graveyard_zone` WHERE `GhostZone`=16651 AND `ID` IN (31,1423);
INSERT INTO `graveyard_zone` (`ID`,`GhostZone`,`Comment`) VALUES
(1423,16651,'Shen''dralas - Desolace, Mannoroc Coven GY'),
(31,16651,'Shen''dralas - Desolace, Ethel Rethor GY');
