-- Classic 1.60 (WoW Forever) mage talent Presence of Mind (12043): the next spell it makes instant uses it up. The core makes no
-- proc data for a spell modifier aura, so without this row its charge was never used.

DELETE FROM `spell_proc` WHERE `SpellId` = 12043;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(12043, 0, 3, 0x40000000, 0, 0, 0, 0x10000, 0, 7, 1, 0, 8, 0, 0, 0, 0, 1); -- Presence of Mind: used by the next mage spell whose cast time it removes
