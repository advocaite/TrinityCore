-- Classic 1.60: Revealed Flaw's crit aura (28815, +100 % crit for the next Backstab, Sinister Strike or Hemorrhage) is a one-charge spell mod, and the
-- charge is only dropped through the proc system, which needs a spell_proc row (see 2026_10_15_01 classic_charge_mod_procs). Without one the aura
-- stayed and every Sinister Strike and Backstab after the first Eviscerate crit (damage_parity 164: 42 of 44 Sinister Strikes critical after one
-- Eviscerate). PROC_ATTR_REQ_SPELLMOD (8): only an ability the mod changed spends the charge. Same shape as Cold Blood (14177).
-- Safe to run again.
DELETE FROM `spell_proc` WHERE `SpellId` = 28815;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`,
`ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(28815, 0, 0, 0, 0, 0, 0, 0x00010, 0, 1, 3, 0, 8, 0, 0, 100, 0, 1);
