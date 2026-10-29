-- Classic 1.60 (WoW Forever) warlock talent Amplify Curse (18288): the next Curse of Weakness or Bane of Agony uses it up. The core
-- makes no proc data for a spell modifier aura, so without this row its charge was never used. It is used in the finish phase,
-- once the curse's effects took its modifier (the core doesn't record a Points modifier on the spell: no PROC_ATTR_REQ_SPELLMOD).

DELETE FROM `spell_proc` WHERE `SpellId` = 18288;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(18288, 0, 5, 0x8400, 0, 0, 0, 0x10000, 0, 7, 4, 0, 0, 0, 0, 0, 0, 0); -- Amplify Curse: used by the next Curse of Weakness or Bane of Agony that hits
