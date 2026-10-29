-- Classic 1.60: Defensive State (5302), the 5 sec state that follows a block, dodge or parry and lights up Revenge, comes from the hidden
-- warrior passive Defensive State (DND) (5301). Its client proc data has no hit mask, so any hit taken, and any spell the warrior cast
-- on themselves, gave it: only blocks, dodges and parries. Its charge was used by any melee ability: only by Revenge (family flag
-- 0x400). Revenge itself checks the core's aura state, which blocks, dodges and parries set. Safe to run again.
DELETE FROM `spell_proc` WHERE `SpellId` IN (5301, 5302);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(5301, 0, 0, 0, 0, 0, 0, 0x2A8, 0, 0, 0, 0x2070, 0, 0, 0, 100, 0, 0),
(5302, 0, 4, 0x400, 0, 0, 0, 0x10, 0, 0x7, 0x1, 0, 0, 0, 0, 100, 0, 0);
