-- Garments of Darkness (5650) and its sibling "Heal and fortify X" quests (5621, 5624, 5625, 5648)
-- Player-cast friendly spells (Flash Heal, Power Word: Fortitude) only accept a creature target when
-- the creature's type flags contain CREATURE_TYPE_FLAG_CAN_ASSIST (0x1000), see WorldObject::IsValidAssistTarget.
-- Without it the cast fails with SPELL_FAILED_BAD_TARGETS and the quest objective can never progress.
-- These NPCs use the DifficultyID 0 row (2026_09_29_01_world_classic_creature_difficulty.sql keeps only that one).
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` | 0x1000 WHERE `Entry` IN (12423, 12427, 12428, 12429, 12430) AND `DifficultyID` = 0;
