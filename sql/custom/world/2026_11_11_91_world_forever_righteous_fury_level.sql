-- Righteous Fury (25780) is trained at level 10 on WoW Forever, not 16 (Dumpins). The Forever client's own spell data says 10
-- (SpellLevels: BaseLevel 10, SpellLevel 10, build 70790); our trainer rows still had the vanilla 16, on every paladin trainer.
-- Safe to run again.
UPDATE `trainer_spell` SET `ReqLevel` = 10 WHERE `SpellId` = 25780;
