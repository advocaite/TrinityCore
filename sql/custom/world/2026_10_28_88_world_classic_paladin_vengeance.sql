-- Classic 1.60: Vengeance (paladin talent 20049) procs from non-periodic critical strikes only. Its client proc data has no hit mask, so
-- every hit gave the damage buff 20050. Safe to run again.
DELETE FROM `spell_proc` WHERE `SpellId` = 20049;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(20049, 0, 0, 0, 0, 0, 0, 0x11154, 0, 0x1, 0x2, 0x2, 0, 0, 0, 100, 0, 0);
