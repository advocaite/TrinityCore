-- Classic 1.60: the hunter's Defensive State (5302, after a dodge: lights up Mongoose Bite) and Defensive State 2 (24948, after a parry:
-- Counterattack) come from the hidden passives 13358 and 24949. Their client proc data has no hit mask, so every hit taken gave both:
-- only dodges and parries. The abilities themselves check the core's aura states. Safe to run again.
DELETE FROM `spell_proc` WHERE `SpellId` IN (13358, 24949);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(13358, 0, 0, 0, 0, 0, 0, 0x28, 0, 0, 0, 0x10, 0, 0, 0, 100, 0, 0),
(24949, 0, 0, 0, 0, 0, 0, 0x28, 0, 0, 0, 0x20, 0, 0, 0, 100, 0, 0);
