-- Classic 1.60: Inner Focus (14751) is used up by the next spell whose cost and critical strike chance it changes (hit phase, so that the
-- crit chance still counts; its spell modifier). It had no proc row, so every spell stayed free until the next death. Safe to run again.
DELETE FROM `spell_proc` WHERE `SpellId` = 14751;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(14751, 0, 6, 0xD4F7FFFB, 0x10940436, 0x5010200, 0x10000490, 0x15550, 0, 0x7, 0x2, 0, 0x8, 0, 0, 100, 0, 0);
