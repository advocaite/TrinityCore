-- Classic 1.60: Sniper Shot (1310687, 1310785, 1310786) gives the next 3 Shots 10 yards of range, but it had no proc row, so its charges
-- were never used. The shots it affects (family flags 0x61A01, Auto Shot included) use them at the cast, with its spell modifier.
-- Safe to run again.
DELETE FROM `spell_proc` WHERE `SpellId` IN (1310687, 1310785, 1310786);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(1310687, 0, 9, 0x61A01, 0, 0, 0, 0x140, 0, 0x7, 0x1, 0, 0x9, 0, 0, 100, 0, 0),
(1310785, 0, 9, 0x61A01, 0, 0, 0, 0x140, 0, 0x7, 0x1, 0, 0x9, 0, 0, 100, 0, 0),
(1310786, 0, 9, 0x61A01, 0, 0, 0, 0x140, 0, 0x7, 0x1, 0, 0x9, 0, 0, 100, 0, 0);
