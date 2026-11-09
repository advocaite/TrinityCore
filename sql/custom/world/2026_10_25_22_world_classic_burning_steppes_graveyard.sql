-- Classic 1.60: Burning Steppes (zone 46) had no vanilla graveyard. VMaNGOS links only graveyard 632, which is missing from
-- world_safe_locs (WorldSafeLocs 632 was dropped before 3.3.5, so the 3.3.5 positions used in 2026_10_25_20 don't have it), so the
-- zone fell back to three retail graveyards (Blackrock Mountain, East, Center East), none of them near a spirit healer (555-1017 y).
-- INFERRED position: the vanilla Flame Crest spirit healer's spawn (VMaNGOS guid 40528 = creature 20040528); the exact 1.12 graveyard
-- point is not in any data we have. Facing from VMaNGOS world_safe_locs_facing (1.76278 rad).
DELETE FROM `world_safe_locs` WHERE `ID` = 632;
INSERT INTO `world_safe_locs` (`ID`,`MapID`,`LocX`,`LocY`,`LocZ`,`Facing`,`Comment`) VALUES
(632,0,-7493.53,-2115.28,136.081,101.0,'Burning Steppes, Flame Crest GY (INFERRED: spirit healer spawn)');
DELETE FROM `graveyard_zone` WHERE `GhostZone` = 46;
INSERT INTO `graveyard_zone` (`ID`,`GhostZone`,`Comment`) VALUES
(632,46,'Burning Steppes - Burning Steppes, Flame Crest GY');
