-- Classic 1.60 (WoW Forever) priest and shaman talents / auras whose client spells have no trigger spell or only a dummy effect.
-- C++: Custom/Classic/world/talents/classic_talents_mage_shaman.cpp. Behaviour from VMaNGOS (GPL-2.0); amounts come from the client spell data.
-- Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('classic_spell_sha_lightning_shield', 'classic_spell_sha_weapon_totem_aura', 'classic_spell_sha_ancestral_healing');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
-- Lightning Shield ranks 1-7: the trigger spell 26545 is a bare dummy, the rank's damage spell is cast on the attacker
(324, 'classic_spell_sha_lightning_shield'),
(325, 'classic_spell_sha_lightning_shield'),
(905, 'classic_spell_sha_lightning_shield'),
(945, 'classic_spell_sha_lightning_shield'),
(8134, 'classic_spell_sha_lightning_shield'),
(10431, 'classic_spell_sha_lightning_shield'),
(10432, 'classic_spell_sha_lightning_shield'),
-- Windfury Totem (8515, 10609, 10612) and Flametongue Totem (8230, 8250, 10521, 15036) passives: no trigger spell in the client effect
(8515, 'classic_spell_sha_weapon_totem_aura'),
(10609, 'classic_spell_sha_weapon_totem_aura'),
(10612, 'classic_spell_sha_weapon_totem_aura'),
(8230, 'classic_spell_sha_weapon_totem_aura'),
(8250, 'classic_spell_sha_weapon_totem_aura'),
(10521, 'classic_spell_sha_weapon_totem_aura'),
(15036, 'classic_spell_sha_weapon_totem_aura'),
-- Ancestral Healing: critical heals give the target 16177
(16176, 'classic_spell_sha_ancestral_healing');

-- Proc data (columns: school, family, family masks, proc flags, flags 2, spell type, phase, hit, attributes, disable effects, ppm, chance, cooldown, charges).
-- Lightning Shield: melee swings, melee abilities and ranged attacks taken (0x2A8), the spell's own 3 charges.
-- Ancestral Healing: critical direct heals (0x4400 = helpful spell / helpful ability done), triggered heals too (attribute 2), like Inspiration.
-- Windfury / Flametongue Totem passives: the party member's melee swings and abilities (0x14), damage that lands, 20% for Windfury from the spell's
-- chance, Flametongue 100%.
-- Shadow Weaving (shadow damage hits), Blackout (shadow damage and non-damage hits) and Elemental Focus (fire, frost and nature damage hits) only proc
-- on their own schools; without a school filter every harmful spell of the priest or shaman rolled them (VMaNGOS spell_proc_event school masks 32 / 28).
DELETE FROM `spell_proc` WHERE `SpellId` IN (324, 325, 905, 945, 8134, 10431, 10432, 16176, 8515, 10609, 10612, 8230, 8250, 10521, 15036, 15257, 15268, 15323, 15324, 15325, 15326, 16164);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`,
`ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(324, 0, 0, 0, 0, 0, 0, 0x2A8, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0),
(325, 0, 0, 0, 0, 0, 0, 0x2A8, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0),
(905, 0, 0, 0, 0, 0, 0, 0x2A8, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0),
(945, 0, 0, 0, 0, 0, 0, 0x2A8, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0),
(8134, 0, 0, 0, 0, 0, 0, 0x2A8, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0),
(10431, 0, 0, 0, 0, 0, 0, 0x2A8, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0),
(10432, 0, 0, 0, 0, 0, 0, 0x2A8, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0),
(16176, 0, 0, 0, 0, 0, 0, 0x4400, 0, 2, 2, 0x2, 2, 0, 0, 0, 0, 0),
(8515, 0, 0, 0, 0, 0, 0, 0x14, 0, 1, 2, 0, 0, 0, 0, 0, 0, 0),
(10609, 0, 0, 0, 0, 0, 0, 0x14, 0, 1, 2, 0, 0, 0, 0, 0, 0, 0),
(10612, 0, 0, 0, 0, 0, 0, 0x14, 0, 1, 2, 0, 0, 0, 0, 0, 0, 0),
(8230, 0, 0, 0, 0, 0, 0, 0x14, 0, 1, 2, 0, 0, 0, 0, 100, 0, 0),
(8250, 0, 0, 0, 0, 0, 0, 0x14, 0, 1, 2, 0, 0, 0, 0, 100, 0, 0),
(10521, 0, 0, 0, 0, 0, 0, 0x14, 0, 1, 2, 0, 0, 0, 0, 100, 0, 0),
(15036, 0, 0, 0, 0, 0, 0, 0x14, 0, 1, 2, 0, 0, 0, 0, 100, 0, 0),
(15257, 32, 0, 0, 0, 0, 0, 0, 0, 1, 2, 0, 0, 0, 0, 0, 0, 0),
(15268, 32, 0, 0, 0, 0, 0, 0, 0, 5, 2, 0, 0, 0, 0, 0, 0, 0),
(15323, 32, 0, 0, 0, 0, 0, 0, 0, 5, 2, 0, 0, 0, 0, 0, 0, 0),
(15324, 32, 0, 0, 0, 0, 0, 0, 0, 5, 2, 0, 0, 0, 0, 0, 0, 0),
(15325, 32, 0, 0, 0, 0, 0, 0, 0, 5, 2, 0, 0, 0, 0, 0, 0, 0),
(15326, 32, 0, 0, 0, 0, 0, 0, 0, 5, 2, 0, 0, 0, 0, 0, 0, 0),
(16164, 28, 0, 0, 0, 0, 0, 0, 0, 1, 2, 0, 0, 0, 0, 0, 0, 0);
