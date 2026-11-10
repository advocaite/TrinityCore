-- Classic 1.60 (WoW Forever): spawned creatures with no model the 70532 client has (server audit 2026-10-24, finding 10).
--
-- Nine spawned templates still point at display ids outside the client's CreatureDisplayInfo (the 2026_10_23_03 pass caught the
-- first twelve); they render as a white box. The VMaNGOS 1.12 display ids for the same entries exist in the 70532 client data,
-- so each template gets that model (checked against the decoded CreatureDisplayInfo id list):
--   2330  Karlee Chaddis        99196 -> 1471      2331  Paige Chaddis          99197 -> 252
--   2756  Chao-Ju               46093 -> 3428      8118  Lillian Singh          99195 -> 7330
--   11497 The Razza             37571 -> 12683     13602 Racksum Greep          52908 -> 13644
--   14869 Pygmy Cockatrice      30970 -> 14940 / 14941 (two skins, as in 1.12)
--   15466 Minion of Omen        59006 -> 15878     15580 Elder Ezra Wheathoof   28420 -> 15627
--
-- Entry 242267 has four spawns from the Forever sniff (2026_10_03_00, build 70170) and no creature_template anywhere (not in the
-- sniff, advocaite's dump or the 2026-10-14 TDB); the server drops them at load with an error. Removed; the positions are kept here:
--   map 0 (2164.17, 449.99, 67.85), (-5644.99, -496.71, 396.74), (-9402.55, -24.31, 64.15); map 1 (137.65, -4716.66, 18.36).

DELETE FROM `creature_template_model` WHERE `CreatureID` IN (2330, 2331, 2756, 8118, 11497, 13602, 14869, 15466, 15580);
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`) VALUES
(2330, 0, 1471, 1, 1, 70532),
(2331, 0, 252, 1, 1, 70532),
(2756, 0, 3428, 1, 1, 70532),
(8118, 0, 7330, 1, 1, 70532),
(11497, 0, 12683, 1, 1, 70532),
(13602, 0, 13644, 1, 1, 70532),
(14869, 0, 14940, 1, 1, 70532),
(14869, 1, 14941, 1, 1, 70532),
(15466, 0, 15878, 1, 1, 70532),
(15580, 0, 15627, 1, 1, 70532);

DELETE FROM `creature` WHERE `id` = 242267 AND `guid` IN (21200055, 21200056, 21200057, 21200549);
