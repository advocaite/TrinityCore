-- Classic 1.60: critical-strike-only procs. Without a spell_proc row the core generates a default (SpellMgr::LoadSpellProcs): every normal or critical
-- hit at the client proc chance. Vanilla rules (talent texts, VMaNGOS spell_proc_event): Vengeance stacks on a critical strike, Reckoning follows a
-- critical strike taken, Frenzy follows a critical strike by the pet. ProcFlags 0 and Chance 0 take the client spell's values. Safe to run again.
DELETE FROM `spell_proc` WHERE `SpellId` IN (20049, 20177, 20784);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`,
`ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(20049, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x2, 0, 0, 0, 0, 0, 0),
(20177, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x2, 0, 0, 0, 0, 0, 0),
(20784, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x2, 0, 0, 0, 0, 0, 0);
