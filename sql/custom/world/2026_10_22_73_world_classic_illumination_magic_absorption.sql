-- Classic 1.60 (WoW Forever) Illumination (paladin) and Magic Absorption (mage): the client auras are dummy / trigger-of-a-dummy auras that nothing
-- handled. C++: Custom/Classic/world/talents/classic_talents_priest_paladin.cpp and classic_talents_mage_shaman.cpp. Behaviour from VMaNGOS
-- (GPL-2.0): Illumination gives back the mana cost of a critical Holy Light / Flash of Light, Magic Absorption 1% of the maximum mana per fully
-- resisted spell. Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('classic_spell_pal_illumination', 'classic_spell_mage_magic_absorption');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(20210, 'classic_spell_pal_illumination'),
(29441, 'classic_spell_mage_magic_absorption');

-- Illumination: critical heals of Holy Light (family mask 0x80000000 + 0x400 in mask 2) and Flash of Light (0x40000000), hit phase, heal type.
-- Magic Absorption: fully resisted harmful spells and abilities taken, any spell type, hit phase.
DELETE FROM `spell_proc` WHERE `SpellId` IN (20210, 29441);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`,
`ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(20210, 0, 10, 0xC0000000, 0, 0, 0, 0x4000, 0, 2, 2, 0x2, 0, 0, 0, 0, 0, 0),
(29441, 0, 0, 0, 0, 0, 0, 0x22000, 0, 0, 0, 0x8, 0, 0, 0, 100, 0, 0);
