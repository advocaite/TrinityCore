-- Classic 1.60: Holy Shield (20925, 20927, 20928) deals its Holy damage and uses a charge on blocked attacks only, as its 70790 tooltip says
-- ("deals $s2 Holy damage for each attack blocked while active ... Each block expends a charge") and as VMaNGOS (procEx BLOCK) and the
-- wowsims Forever sim (OutcomeBlock) have it. The client proc data has no hit mask, so without a row the core used the default taken hit
-- mask and every normal or critical hit used a charge. Puts back the rows of 2026_10_25_92 that 2026_11_05_08 held back.
-- ProcFlags, Chance, Cooldown and Charges 0 keep the client's. Safe to run again.
DELETE FROM `spell_proc` WHERE `SpellId` IN (20925, 20927, 20928);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`,
`ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(20925, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x40, 0, 0, 0, 0, 0, 0),
(20927, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x40, 0, 0, 0, 0, 0, 0),
(20928, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x40, 0, 0, 0, 0, 0, 0);
