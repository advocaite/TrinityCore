-- Druid Clearcasting 16870 (Omen of Clarity): the spell_proc row was the retail Restoration one (helpful spells only, heal type, cast phase),
-- so a cat or bear ability got the free cost but never used up the charge. Classic row like mage Clearcasting 12536: melee and ranged
-- abilities plus harmful and helpful spells (no auto attacks), one charge, PROC_ATTR_REQ_SPELLMOD so only a spell the cost mod changed drops it. Safe to run again.
DELETE FROM `spell_proc` WHERE `SpellId` = 16870;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`,
`ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(16870, 0, 0, 0, 0, 0, 0, 0x15510, 0, 7, 3, 0, 8, 0, 0, 100, 0, 1); -- Clearcasting (druid): the next ability or spell is free, in or out of form
