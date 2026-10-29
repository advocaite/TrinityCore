-- Classic 1.60 (WoW Forever) warlock talent Fel Domination (18708): the next demon summon uses it up. The core makes no proc data
-- for a spell modifier aura, so without this row its charge was never used.

DELETE FROM `spell_proc` WHERE `SpellId` = 18708;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(18708, 0, 5, 0x20000000, 0, 0, 0, 0x4000, 0, 7, 1, 0, 8, 0, 0, 0, 0, 0); -- Fel Domination: used by the next summon whose cast time it changes
