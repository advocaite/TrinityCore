-- Classic 1.60: Overpower is usable only after the target dodges. Each Overpower takes an activation (a combo point), which the
-- hidden warrior passive Offensive State (1282735) gives through 1282733. Its client proc data has no hit mask, so every melee hit
-- gave one: only dodged attacks. Safe to run again.
DELETE FROM `spell_proc` WHERE `SpellId` = 1282735;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(1282735, 0, 0, 0, 0, 0, 0, 0x14, 0, 0x7, 0x2, 0x10, 0, 0, 0, 100, 0, 0);
