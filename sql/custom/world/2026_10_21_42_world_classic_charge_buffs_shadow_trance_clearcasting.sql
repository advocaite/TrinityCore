-- Classic one-charge buffs, same rows as 2026_10_15_01 (PROC_ATTR_REQ_SPELLMOD: only a spell the mod changes drops the charge; cast and hit phase).
-- Shadow Trance 17941 has proc charges 0 in the client data, so the aura never lost a charge and every Shadow Bolt for its 10 s was instant instead of
-- only the next one; the row gives it its one charge. Clearcasting 12536 (mage) has one charge in the data but no proc row of ours. Safe to run again.
DELETE FROM `spell_proc` WHERE `SpellId` IN (17941, 12536);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`,
`ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(17941, 0, 0, 0, 0, 0, 0, 0x15400, 0, 7, 3, 0, 8, 0, 0, 100, 0, 1), -- Shadow Trance: the next Shadow Bolt is instant
(12536, 0, 0, 0, 0, 0, 0, 0x15400, 0, 7, 3, 0, 8, 0, 0, 100, 0, 1); -- Clearcasting (mage): the next spell is free
