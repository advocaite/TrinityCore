-- Classic 1.60: Cold Blood (14177) is used up by the next Sinister Strike, Backstab, Ambush, Eviscerate or Mutilate whose critical strike
-- chance it raised (hit phase, its spell modifier, the triggered Mutilate strikes too). It had no proc row, so it lasted until the next
-- death. Safe to run again.
DELETE FROM `spell_proc` WHERE `SpellId` = 14177;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(14177, 0, 8, 0x20206, 0x6, 0, 0, 0x10, 0, 0x1, 0x2, 0, 0x9, 0, 0, 100, 0, 0);
