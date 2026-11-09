-- Racial abilities and traits from the WoW Forever beta development notes of 2026-10-26, world side (the spell data is in the
-- hotfixes of 2026_10_27_05). Safe to run again.

-- Human: The Human Spirit 20598 is now 10% Spirit. The vanilla value row (4 + 1d1 = 5%) is below Forever's, so it goes.
DELETE FROM `classic_spell_value` WHERE `spell_id` = 20598 AND `effect_index` = 0;

-- Orc: Command (20574, Savage renamed by the hotfixes) is a dummy aura; its pet aura gives Hunter and Warlock pets 5% damage done
-- (Pet::CastPetAuras casts it on permanent pets only). 9970801 is a server-side spell: passive, hidden, no combat log.
DELETE FROM `serverside_spell` WHERE `Id` = 9970801;
INSERT INTO `serverside_spell` (`Id`, `DifficultyID`, `Attributes`, `CastingTimeIndex`, `ProcChance`, `DurationIndex`, `RangeIndex`, `EquippedItemClass`, `SpellName`, `SpellFamilyName`, `SchoolMask`) VALUES
(9970801, 0, 448, 1, 0, 21, 1, -1, 'Command (pet)', 0, 1);
DELETE FROM `serverside_spell_effect` WHERE `SpellID` = 9970801;
INSERT INTO `serverside_spell_effect` (`SpellID`, `EffectIndex`, `DifficultyID`, `Effect`, `EffectAura`, `EffectBasePoints`, `EffectMiscValue1`, `ImplicitTarget1`) VALUES
(9970801, 0, 0, 6, 79, 5, 127, 1);
DELETE FROM `spell_pet_auras` WHERE `spell` = 20574;
INSERT INTO `spell_pet_auras` (`spell`, `effectId`, `pet`, `aura`) VALUES
(20574, 0, 0, 9970801);

-- Skyborne: Innervate replaces Read Leyline's Energized as the more powerful effect.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_innervate_replaces_energized';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(29166, 'classic_spell_innervate_replaces_energized'),
(456195, 'classic_spell_innervate_replaces_energized');
