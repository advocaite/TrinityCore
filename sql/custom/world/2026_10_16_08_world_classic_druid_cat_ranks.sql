-- Classic 1.60: the druid cat line is trainable again.
-- 2026_10_15_00 moved every druid trainer onto the shared vanilla list 17, but Cat Form, Claw, Rip, Prowl
-- and Tiger's Fury (rank 1) only existed on the retail per-creature lists (1003033, 1003034, ...), so no
-- druid trainer offered them. Costs and levels are the live per-creature rows.
INSERT IGNORE INTO `trainer_spell` (`TrainerId`, `SpellId`, `MoneyCost`, `ReqSkillLine`, `ReqSkillRank`, `ReqAbility1`, `ReqAbility2`, `ReqAbility3`, `ReqLevel`, `VerifiedBuild`) VALUES
(17, 768, 2000, 0, 0, 0, 0, 0, 20, 0),   -- Cat Form
(17, 1082, 2000, 0, 0, 0, 0, 0, 20, 0),  -- Claw (Rank 1)
(17, 1079, 2000, 0, 0, 0, 0, 0, 20, 0),  -- Rip (Rank 1)
(17, 5215, 2000, 0, 0, 0, 0, 0, 20, 0),  -- Prowl (Rank 1)
(17, 5217, 4000, 0, 0, 0, 0, 0, 24, 0);  -- Tiger's Fury (Rank 1)
