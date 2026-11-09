-- Classic 1.60: the official priest trainer list (8, 2026_10_15_00_world_classic_class_trainers.sql) teaches no rank above 1 of the
-- vanilla racial spells, although the client has every rank: a level-40 Troll priest keeps Shadowguard rank 1 and Hex of Weakness
-- rank 1, a Night Elf keeps Starshards rank 1, Lightwell never grows past rank 1. Levels, prices and required ranks are the vanilla
-- trainer values (the same rows the old Zayus list 1005994 carries). Rank 1 of each is a starting spell and stays as it is.
-- Runs after 2026_10_15_00 (which rebuilds list 8). Safe to run again.
INSERT IGNORE INTO `trainer_spell` (`TrainerId`, `SpellId`, `MoneyCost`, `ReqSkillLine`, `ReqSkillRank`, `ReqAbility1`, `ReqAbility2`, `ReqAbility3`, `ReqLevel`, `VerifiedBuild`) VALUES
-- Touch of Weakness (Undead) ranks 2-6
(8, 19261, 150, 0, 0, 2652, 0, 0, 20, 0),
(8, 19262, 500, 0, 0, 19261, 0, 0, 30, 0),
(8, 19264, 900, 0, 0, 19262, 0, 0, 40, 0),
(8, 19265, 1500, 0, 0, 19264, 0, 0, 50, 0),
(8, 19266, 2300, 0, 0, 19265, 0, 0, 60, 0),
-- Hex of Weakness (Troll) ranks 2-6
(8, 19281, 150, 0, 0, 9035, 0, 0, 20, 0),
(8, 19282, 500, 0, 0, 19281, 0, 0, 30, 0),
(8, 19283, 900, 0, 0, 19282, 0, 0, 40, 0),
(8, 19284, 1500, 0, 0, 19283, 0, 0, 50, 0),
(8, 19285, 2300, 0, 0, 19284, 0, 0, 60, 0),
-- Starshards (Night Elf) ranks 2-7
(8, 19296, 100, 0, 0, 10797, 0, 0, 18, 0),
(8, 19299, 300, 0, 0, 19296, 0, 0, 26, 0),
(8, 19302, 600, 0, 0, 19299, 0, 0, 34, 0),
(8, 19303, 1100, 0, 0, 19302, 0, 0, 42, 0),
(8, 19304, 1500, 0, 0, 19303, 0, 0, 50, 0),
(8, 19305, 2200, 0, 0, 19304, 0, 0, 58, 0),
-- Shadowguard (Troll) ranks 2-6
(8, 19308, 400, 0, 0, 18137, 0, 0, 28, 0),
(8, 19309, 700, 0, 0, 19308, 0, 0, 36, 0),
(8, 19310, 1200, 0, 0, 19309, 0, 0, 44, 0),
(8, 19311, 1900, 0, 0, 19310, 0, 0, 52, 0),
(8, 19312, 2300, 0, 0, 19311, 0, 0, 60, 0),
-- Lightwell ranks 2-3 (rank 1 is the talent, 724)
(8, 27870, 1200, 0, 0, 724, 0, 0, 50, 0),
(8, 27871, 1500, 0, 0, 27870, 0, 0, 60, 0);
