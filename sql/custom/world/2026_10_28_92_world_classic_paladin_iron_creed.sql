-- Classic 1.60: Iron Creed (paladin talent 1311034) reduces the damage taken after a Holy Strike (family flag 0x8000 in the 4th mask)
-- while Righteous Fury is active (classic_talents_priest_paladin.cpp). Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` = 1311034 AND `ScriptName` = 'classic_spell_pal_iron_creed';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(1311034, 'classic_spell_pal_iron_creed');

DELETE FROM `spell_proc` WHERE `SpellId` = 1311034;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(1311034, 0, 10, 0, 0, 0, 0x8000, 0x10, 0, 0x1, 0x2, 0, 0, 0x1, 0, 100, 0, 0);
