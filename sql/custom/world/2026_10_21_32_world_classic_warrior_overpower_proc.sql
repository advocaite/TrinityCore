-- Warrior Overpower. The client data (spell dump, 140) has 1282735 "Offensive State (DND)": aura 42 (proc trigger spell), proc chance 100, triggering 1282733
-- "Overpower" (energize one combo point + a short aura). With no spell_proc row it procs on every landed hit (61 combo points in a run with 0 dodges, 138),
-- so Overpower was always castable. Restrict it to a dodge of a melee swing or melee ability (vMaNGOS: Unit::ProcDamageAndSpell sets REACTIVE_OVERPOWER on
-- a target dodge; Overpower is usable only then). PROC_FLAG_DEAL_MELEE_SWING | PROC_FLAG_DEAL_MELEE_ABILITY, PROC_HIT_DODGE; spell type damage|heal|no-damage
-- (7) and the hit phase (2), which SpellMgr::CanSpellTriggerProcOnEvent requires for the ability flag (a phase mask of 0 rejects every ability event).
DELETE FROM `spell_proc` WHERE `SpellId` = 1282735;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`,
`ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(1282735, 0, 0, 0, 0, 0, 0, 0x14, 0, 0x7, 0x2, 0x10, 0, 0, 0, 100, 0, 0);
