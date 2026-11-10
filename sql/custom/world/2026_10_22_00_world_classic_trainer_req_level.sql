-- Class trainers: rows sniffed with ReqLevel 0 get the level the spell is taught at. Trainer::GetSpellState gates on ReqLevel only, so with 0 a rank 2
-- Rejuvenation or Healing Wave was trainable at level 1 once rank 1 was known. Levels are the vMaNGOS 1.12 trainer levels (GPL-2.0, vmangos/core npc_trainer_template),
-- which equal the spells' own levels in the client data. Rows already above 0 are not touched (WHERE ReqLevel = 0).

UPDATE `trainer_spell` SET `ReqLevel` = 6 WHERE `TrainerId` = 10 AND `SpellId` = 332 AND `ReqLevel` = 0; -- Healing Wave
UPDATE `trainer_spell` SET `ReqLevel` = 6 WHERE `TrainerId` = 10 AND `SpellId` = 2484 AND `ReqLevel` = 0; -- Earthbind Totem
UPDATE `trainer_spell` SET `ReqLevel` = 1 WHERE `TrainerId` = 10 AND `SpellId` = 8017 AND `ReqLevel` = 0; -- Rockbiter Weapon
UPDATE `trainer_spell` SET `ReqLevel` = 4 WHERE `TrainerId` = 10 AND `SpellId` = 8042 AND `ReqLevel` = 0; -- Earth Shock
UPDATE `trainer_spell` SET `ReqLevel` = 1 WHERE `TrainerId` = 11 AND `SpellId` = 8017 AND `ReqLevel` = 0; -- Rockbiter Weapon
UPDATE `trainer_spell` SET `ReqLevel` = 4 WHERE `TrainerId` = 11 AND `SpellId` = 8042 AND `ReqLevel` = 0; -- Earth Shock
UPDATE `trainer_spell` SET `ReqLevel` = 10 WHERE `TrainerId` = 17 AND `SpellId` = 99 AND `ReqLevel` = 0; -- Demoralizing Roar
UPDATE `trainer_spell` SET `ReqLevel` = 8 WHERE `TrainerId` = 17 AND `SpellId` = 339 AND `ReqLevel` = 0; -- Entangling Roots
UPDATE `trainer_spell` SET `ReqLevel` = 6 WHERE `TrainerId` = 17 AND `SpellId` = 467 AND `ReqLevel` = 0; -- Thorns
UPDATE `trainer_spell` SET `ReqLevel` = 4 WHERE `TrainerId` = 17 AND `SpellId` = 774 AND `ReqLevel` = 0; -- Rejuvenation
UPDATE `trainer_spell` SET `ReqLevel` = 14 WHERE `TrainerId` = 17 AND `SpellId` = 782 AND `ReqLevel` = 0; -- Thorns
UPDATE `trainer_spell` SET `ReqLevel` = 10 WHERE `TrainerId` = 17 AND `SpellId` = 1058 AND `ReqLevel` = 0; -- Rejuvenation
UPDATE `trainer_spell` SET `ReqLevel` = 1 WHERE `TrainerId` = 17 AND `SpellId` = 1126 AND `ReqLevel` = 0; -- Mark of the Wild
UPDATE `trainer_spell` SET `ReqLevel` = 6 WHERE `TrainerId` = 17 AND `SpellId` = 5177 AND `ReqLevel` = 0; -- Wrath
UPDATE `trainer_spell` SET `ReqLevel` = 14 WHERE `TrainerId` = 17 AND `SpellId` = 5178 AND `ReqLevel` = 0; -- Wrath
UPDATE `trainer_spell` SET `ReqLevel` = 8 WHERE `TrainerId` = 17 AND `SpellId` = 5186 AND `ReqLevel` = 0; -- Healing Touch
UPDATE `trainer_spell` SET `ReqLevel` = 14 WHERE `TrainerId` = 17 AND `SpellId` = 5187 AND `ReqLevel` = 0; -- Healing Touch
UPDATE `trainer_spell` SET `ReqLevel` = 14 WHERE `TrainerId` = 17 AND `SpellId` = 5211 AND `ReqLevel` = 0; -- Bash
UPDATE `trainer_spell` SET `ReqLevel` = 12 WHERE `TrainerId` = 17 AND `SpellId` = 5229 AND `ReqLevel` = 0; -- Enrage
UPDATE `trainer_spell` SET `ReqLevel` = 10 WHERE `TrainerId` = 17 AND `SpellId` = 5232 AND `ReqLevel` = 0; -- Mark of the Wild
UPDATE `trainer_spell` SET `ReqLevel` = 4 WHERE `TrainerId` = 17 AND `SpellId` = 8921 AND `ReqLevel` = 0; -- Moonfire
UPDATE `trainer_spell` SET `ReqLevel` = 10 WHERE `TrainerId` = 17 AND `SpellId` = 8924 AND `ReqLevel` = 0; -- Moonfire
UPDATE `trainer_spell` SET `ReqLevel` = 12 WHERE `TrainerId` = 17 AND `SpellId` = 8936 AND `ReqLevel` = 0; -- Regrowth
UPDATE `trainer_spell` SET `ReqLevel` = 10 WHERE `TrainerId` = 17 AND `SpellId` = 16689 AND `ReqLevel` = 0; -- Nature's Grasp
UPDATE `trainer_spell` SET `ReqLevel` = 12 WHERE `TrainerId` = 17 AND `SpellId` = 437138 AND `ReqLevel` = 0; -- Revive
UPDATE `trainer_spell` SET `ReqLevel` = 1 WHERE `TrainerId` = 20 AND `SpellId` = 1494 AND `ReqLevel` = 0; -- Track Beasts
UPDATE `trainer_spell` SET `ReqLevel` = 1 WHERE `TrainerId` = 28 AND `SpellId` = 465 AND `ReqLevel` = 0; -- Devotion Aura
UPDATE `trainer_spell` SET `ReqLevel` = 4 WHERE `TrainerId` = 28 AND `SpellId` = 19740 AND `ReqLevel` = 0; -- Blessing of Might
UPDATE `trainer_spell` SET `ReqLevel` = 1 WHERE `TrainerId` = 29 AND `SpellId` = 465 AND `ReqLevel` = 0; -- Devotion Aura
UPDATE `trainer_spell` SET `ReqLevel` = 6 WHERE `TrainerId` = 29 AND `SpellId` = 498 AND `ReqLevel` = 0; -- Divine Protection
UPDATE `trainer_spell` SET `ReqLevel` = 6 WHERE `TrainerId` = 29 AND `SpellId` = 639 AND `ReqLevel` = 0; -- Holy Light
UPDATE `trainer_spell` SET `ReqLevel` = 99 WHERE `TrainerId` = 29 AND `SpellId` = 679 AND `ReqLevel` = 0; -- zzOLDHoly Strike
UPDATE `trainer_spell` SET `ReqLevel` = 8 WHERE `TrainerId` = 29 AND `SpellId` = 853 AND `ReqLevel` = 0; -- Hammer of Justice
UPDATE `trainer_spell` SET `ReqLevel` = 8 WHERE `TrainerId` = 29 AND `SpellId` = 1152 AND `ReqLevel` = 0; -- Purify
UPDATE `trainer_spell` SET `ReqLevel` = 4 WHERE `TrainerId` = 29 AND `SpellId` = 19740 AND `ReqLevel` = 0; -- Blessing of Might
UPDATE `trainer_spell` SET `ReqLevel` = 6 WHERE `TrainerId` = 29 AND `SpellId` = 21082 AND `ReqLevel` = 0; -- Seal of the Crusader
UPDATE `trainer_spell` SET `ReqLevel` = 1 WHERE `TrainerId` = 1233 AND `SpellId` = 1126 AND `ReqLevel` = 0; -- Mark of the Wild
