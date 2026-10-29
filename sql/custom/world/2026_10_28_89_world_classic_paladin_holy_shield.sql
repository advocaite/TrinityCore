-- Classic 1.60: Holy Shield (20925, 20927, 20928) deals its Holy damage, and uses a charge, only when the paladin blocks. Its client proc
-- data has no hit mask, so every hit taken dealt the damage and used one of its 4 charges. Safe to run again.
DELETE FROM `spell_proc` WHERE `SpellId` IN (20925, 20927, 20928);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(20925, 0, 0, 0, 0, 0, 0, 0x2A8, 0, 0, 0, 0x2040, 0, 0, 0, 100, 0, 0),
(20927, 0, 0, 0, 0, 0, 0, 0x2A8, 0, 0, 0, 0x2040, 0, 0, 0, 100, 0, 0),
(20928, 0, 0, 0, 0, 0, 0, 0x2A8, 0, 0, 0, 0x2040, 0, 0, 0, 100, 0, 0);
