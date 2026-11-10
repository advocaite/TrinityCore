-- Classic 1.60 Impact (11103): the client spell is a proc-trigger aura (stun 12355) with 10 in its amount and 100 as spell proc chance, so
-- without a proc row the stun could fire on every Fire spell. Fire spell damage hits proc it with a chance of 10. Safe to run again.
DELETE FROM `spell_proc` WHERE `SpellId` = 11103;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(11103, 4, 0, 0, 0, 0, 0, 0x10000, 0, 1, 2, 0, 0, 0, 0, 10, 0, 0); -- Fire spell damage hits, 10 percent
