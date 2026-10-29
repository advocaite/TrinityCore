-- Classic 1.60: Illumination (paladin talent 20210) gives mana after critical heals of Flash of Light, Holy Light, Holy Shock and Light's
-- Vigil (family flags 0x40000000, 0x80000000, 0x200000 and 0x4000000 in the 4th mask; the triggered heals too), at its rank's chance
-- (classic_talents_priest_paladin.cpp). Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` = 20210 AND `ScriptName` = 'classic_spell_pal_illumination';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(20210, 'classic_spell_pal_illumination');

DELETE FROM `spell_proc` WHERE `SpellId` = 20210;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(20210, 0, 10, 0xC0200000, 0, 0, 0x4000000, 0x4000, 0, 0x2, 0x2, 0x2, 0x1, 0, 0, 100, 0, 0);
