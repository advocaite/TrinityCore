-- Vanilla 1.12 trainer spells whose ids are valid client spells but were missing from the sniffed class trainer lists
-- (levels and costs from vMaNGOS 1.12, GPL-2.0). Race and class gating stays with Player::IsSpellFitByClassAndRace, so
-- the racial Desperate Prayer ranks only show for Human and Dwarf priests and each faction sees its own city teleports.
-- Spells that vanilla taught but the client data does not contain (Tiger's Fury r2-r4, Multi-Shot r2-r4, ...) cannot be taught.

DELETE FROM `trainer_spell` WHERE `TrainerId` = 8 AND `SpellId` IN (19236, 19238, 19240, 19241, 19242, 19243);
INSERT INTO `trainer_spell` (`TrainerId`, `SpellId`, `MoneyCost`, `ReqSkillLine`, `ReqSkillRank`, `ReqAbility1`, `ReqAbility2`, `ReqAbility3`, `ReqLevel`, `VerifiedBuild`) VALUES
(8, 19236, 100, 0, 0, 13908, 0, 0, 18, 0),
(8, 19238, 300, 0, 0, 19236, 0, 0, 26, 0),
(8, 19240, 600, 0, 0, 19238, 0, 0, 34, 0),
(8, 19241, 1100, 0, 0, 19240, 0, 0, 42, 0),
(8, 19242, 1500, 0, 0, 19241, 0, 0, 50, 0),
(8, 19243, 2200, 0, 0, 19242, 0, 0, 58, 0);

DELETE FROM `trainer_spell` WHERE `TrainerId` = 1 AND `SpellId` IN (3561, 3562, 3563, 3565, 3566, 3567, 10059, 11416, 11417, 11418, 11419, 11420);
INSERT INTO `trainer_spell` (`TrainerId`, `SpellId`, `MoneyCost`, `ReqSkillLine`, `ReqSkillRank`, `ReqAbility1`, `ReqAbility2`, `ReqAbility3`, `ReqLevel`, `VerifiedBuild`) VALUES
(1, 3561, 2000, 0, 0, 0, 0, 0, 20, 0),
(1, 3562, 2000, 0, 0, 0, 0, 0, 20, 0),
(1, 3563, 2000, 0, 0, 0, 0, 0, 20, 0),
(1, 3567, 2000, 0, 0, 0, 0, 0, 20, 0),
(1, 3565, 8000, 0, 0, 0, 0, 0, 30, 0),
(1, 3566, 8000, 0, 0, 0, 0, 0, 30, 0),
(1, 10059, 15000, 0, 0, 0, 0, 0, 40, 0),
(1, 11416, 15000, 0, 0, 0, 0, 0, 40, 0),
(1, 11417, 15000, 0, 0, 0, 0, 0, 40, 0),
(1, 11418, 15000, 0, 0, 0, 0, 0, 40, 0),
(1, 11419, 32000, 0, 0, 0, 0, 0, 50, 0),
(1, 11420, 32000, 0, 0, 0, 0, 0, 50, 0);
