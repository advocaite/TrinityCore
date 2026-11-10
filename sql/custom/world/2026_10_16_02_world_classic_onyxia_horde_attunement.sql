-- Onyxia attunement, Horde: 4903 "Warlord's Command" opens the chain (4903 -> 4941 -> 4974 -> 6566 ... -> 6602, Drakefire Amulet 16309).
-- The baseline (2026_09_27_00_world_forever_baseline_01.sql) only kept Warlord Goretooth (9077, Kargath) as its ender, so no Horde
-- character could start the chain; the rest of it (Thrall 4949, Rexxar 10182, Myranda 11872, Emberstrife 10321) is in place.
-- Vanilla: Goretooth starts and ends 4903. Safe to run again.
INSERT IGNORE INTO `creature_queststarter` (`id`, `quest`, `VerifiedBuild`) VALUES (9077, 4903, 0);
