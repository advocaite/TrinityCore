-- Forever trainers that could not train, from a sweep of every NPC Wowhead Forever tags as a trainer against our spawns
-- (follow-up to 2026_10_27_00).
-- Frost Blast (1326053-1326055, Forever mage spell, SkillLineAbility skill 6 learned from a trainer) was on no trainer of ours;
-- The other Forever server has it on every full mage trainer, with these costs and levels. Added to ours (trainer 1 and the full 148-spell lists).
DELETE FROM `trainer_spell` WHERE `SpellId` IN (1326053,1326054,1326055);
INSERT INTO `trainer_spell` (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
(1,1326053,4000,0,0,0,0,0,24,0),
(1,1326054,13000,0,0,1326053,0,0,36,0),
(1,1326055,28000,0,0,1326054,0,0,48,0),
(1000328,1326053,4000,0,0,0,0,0,24,0),
(1000328,1326054,13000,0,0,1326053,0,0,36,0),
(1000328,1326055,28000,0,0,1326054,0,0,48,0),
(1000331,1326053,4000,0,0,0,0,0,24,0),
(1000331,1326054,13000,0,0,1326053,0,0,36,0),
(1000331,1326055,28000,0,0,1326054,0,0,48,0),
(1001228,1326053,4000,0,0,0,0,0,24,0),
(1001228,1326054,13000,0,0,1326053,0,0,36,0),
(1001228,1326055,28000,0,0,1326054,0,0,48,0),
(1002128,1326053,4000,0,0,0,0,0,24,0),
(1002128,1326054,13000,0,0,1326053,0,0,36,0),
(1002128,1326055,28000,0,0,1326054,0,0,48,0),
(1003047,1326053,4000,0,0,0,0,0,24,0),
(1003047,1326054,13000,0,0,1326053,0,0,36,0),
(1003047,1326055,28000,0,0,1326054,0,0,48,0),
(1003048,1326053,4000,0,0,0,0,0,24,0),
(1003048,1326054,13000,0,0,1326053,0,0,36,0),
(1003048,1326055,28000,0,0,1326054,0,0,48,0),
(1003049,1326053,4000,0,0,0,0,0,24,0),
(1003049,1326054,13000,0,0,1326053,0,0,36,0),
(1003049,1326055,28000,0,0,1326054,0,0,48,0),
(1004566,1326053,4000,0,0,0,0,0,24,0),
(1004566,1326054,13000,0,0,1326053,0,0,36,0),
(1004566,1326055,28000,0,0,1326054,0,0,48,0),
(1004567,1326053,4000,0,0,0,0,0,24,0),
(1004567,1326054,13000,0,0,1326053,0,0,36,0),
(1004567,1326055,28000,0,0,1326054,0,0,48,0),
(1004568,1326053,4000,0,0,0,0,0,24,0),
(1004568,1326054,13000,0,0,1326053,0,0,36,0),
(1004568,1326055,28000,0,0,1326054,0,0,48,0),
(1005144,1326053,4000,0,0,0,0,0,24,0),
(1005144,1326054,13000,0,0,1326053,0,0,36,0),
(1005144,1326055,28000,0,0,1326054,0,0,48,0),
(1005145,1326053,4000,0,0,0,0,0,24,0),
(1005145,1326054,13000,0,0,1326053,0,0,36,0),
(1005145,1326055,28000,0,0,1326054,0,0,48,0),
(1005146,1326053,4000,0,0,0,0,0,24,0),
(1005146,1326054,13000,0,0,1326053,0,0,36,0),
(1005146,1326055,28000,0,0,1326054,0,0,48,0),
(1005497,1326053,4000,0,0,0,0,0,24,0),
(1005497,1326054,13000,0,0,1326053,0,0,36,0),
(1005497,1326055,28000,0,0,1326054,0,0,48,0),
(1005498,1326053,4000,0,0,0,0,0,24,0),
(1005498,1326054,13000,0,0,1326053,0,0,36,0),
(1005498,1326055,28000,0,0,1326054,0,0,48,0),
(1005880,1326053,4000,0,0,0,0,0,24,0),
(1005880,1326054,13000,0,0,1326053,0,0,36,0),
(1005880,1326055,28000,0,0,1326054,0,0,48,0),
(1005882,1326053,4000,0,0,0,0,0,24,0),
(1005882,1326054,13000,0,0,1326053,0,0,36,0),
(1005882,1326055,28000,0,0,1326054,0,0,48,0),
(1005883,1326053,4000,0,0,0,0,0,24,0),
(1005883,1326054,13000,0,0,1326053,0,0,36,0),
(1005883,1326055,28000,0,0,1326054,0,0,48,0),
(1005885,1326053,4000,0,0,0,0,0,24,0),
(1005885,1326054,13000,0,0,1326053,0,0,36,0),
(1005885,1326055,28000,0,0,1326054,0,0,48,0),
(1007311,1326053,4000,0,0,0,0,0,24,0),
(1007311,1326054,13000,0,0,1326053,0,0,36,0),
(1007311,1326055,28000,0,0,1326054,0,0,48,0),
(1007312,1326053,4000,0,0,0,0,0,24,0),
(1007312,1326054,13000,0,0,1326053,0,0,36,0),
(1007312,1326055,28000,0,0,1326054,0,0,48,0);

-- Class and pet trainers missing the trainer flag, the trainer, or both. Each gets our trainer whose list matches the other Forever server's:
-- paladin 29 (the other Forever server's 152 spells; ours teaches Judgement as 10321 where the other Forever server lists 20271), shaman 10, rogue 26,
-- hunter pet 273 (the other Forever server's 543 has two more spells, 1853 and 14922, that none of our pet trainers teach).
UPDATE `creature_template` SET `npcflag` = `npcflag` | 48 -- UNIT_NPC_FLAG_TRAINER | UNIT_NPC_FLAG_TRAINER_CLASS
WHERE `entry` IN (246344,246389,248415,262558);
UPDATE `creature_template` SET `npcflag` = `npcflag` | 16 -- UNIT_NPC_FLAG_TRAINER
WHERE `entry` IN (3702,205729);

DELETE FROM `creature_trainer` WHERE `CreatureID` IN (3702,205729,246344,262558);
INSERT INTO `creature_trainer` (`CreatureID`,`TrainerID`,`MenuID`,`OptionID`) VALUES
(3702,273,0,0),     -- Alanndarian Nightsong, pet trainer (Darnassus)
(205729,26,0,0),    -- Boarton Shadetotem, rogue
(246344,29,0,0),    -- Alodan the Hopeful, paladin
(262558,10,0,0);    -- Palah Thunderhoof, shaman
-- Hilda the Breaker (246389, paladin 29) and Tordrin Sternblade (248415, hunter 19) already had their trainer and only lacked the flag.
-- Not changed: Ember Bladewhisper (260080, weapons trainer) has no trainer in any source; the demon, owl, wintersaber and ravasaur
-- "trainers" Wowhead lists are vendors or quest NPCs with no trainer in the other Forever server either.
