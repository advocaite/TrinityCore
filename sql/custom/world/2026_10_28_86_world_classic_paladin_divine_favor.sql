-- Classic 1.60: Divine Favor (20216) is used up by the next Flash of Light, Holy Light or Holy Shock whose critical strike chance it
-- raised (hit phase, its spell modifier, triggered Holy Shock heals and damage too). It had no proc row, so it lasted until the next
-- death. Safe to run again.
DELETE FROM `spell_proc` WHERE `SpellId` = 20216;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(20216, 0, 10, 0x80206000, 0, 0, 0, 0x14000, 0, 0x3, 0x2, 0, 0x9, 0, 0, 100, 0, 0);
