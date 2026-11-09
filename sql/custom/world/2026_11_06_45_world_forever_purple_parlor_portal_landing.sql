-- Dalaran: Portal to The Purple Parlor 565745 (spell 1318270) landed the player 3 yards east of the Parlor's own portal 691558
-- (467.36 364.13 292.46). That portal hangs on the room's east wall (the other sniffed objects in the room are west of it, at x 442
-- to 462), so the landing spot was outside the tower and the player fell to their death (player report 2026-11-05). Now 3 yards
-- west of it, inside the room, facing away from the wall.
DELETE FROM `spell_target_position` WHERE `ID` = 1318270 AND `EffectIndex` = 0;
INSERT INTO `spell_target_position` (`ID`,`EffectIndex`,`OrderIndex`,`MapID`,`PositionX`,`PositionY`,`PositionZ`,`Orientation`,`VerifiedBuild`) VALUES
(1318270,0,0,0,464.36,364.13,292.46,3.1416,0);
