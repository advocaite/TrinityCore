-- Classic one-charge cost, cast-time and crit mods that the client data gives no proc flags: the charge is only dropped through the proc
-- system, which needs a spell_proc row, so without one the aura stays for good and every spell it affects keeps the bonus (n43 onyxia_full:
-- healer priests cast 100+ heals at no mana with Inner Focus up; Clearcasting 16870 has a row and druids pay normally). None of the ids
-- below had a row in world.spell_proc (read-only query, 2026-10-15).
-- PROC_ATTR_REQ_SPELLMOD (8): only a spell the mod changed drops the charge, so the broad ProcFlags (helpful and harmful spells and
-- abilities, cast and hit phase) cannot spend it on an unrelated spell. Charges and chance come from the spell (1 and 100).
-- Safe to run again.

DELETE FROM `spell_proc` WHERE `SpellId` IN (12043, 14177, 14751, 16188, 17116, 18288, 18708, 20216);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`,
`ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
-- Presence of Mind: the next mage spell with a cast time is instant
(12043, 0, 0, 0, 0, 0, 0, 0x15400, 0, 7, 3, 0, 8, 0, 0, 100, 0, 1),
-- Cold Blood: the next rogue offensive ability is a critical hit (melee abilities)
(14177, 0, 0, 0, 0, 0, 0, 0x00010, 0, 1, 3, 0, 8, 0, 0, 100, 0, 1),
-- Inner Focus: the next priest spell costs no mana and crits 25 % more
(14751, 0, 0, 0, 0, 0, 0, 0x15400, 0, 7, 3, 0, 8, 0, 0, 100, 0, 1),
-- Nature's Swiftness (druid, shaman): the next nature spell is instant
(16188, 0, 0, 0, 0, 0, 0, 0x15400, 0, 7, 3, 0, 8, 0, 0, 100, 0, 1),
(17116, 0, 0, 0, 0, 0, 0, 0x15400, 0, 7, 3, 0, 8, 0, 0, 100, 0, 1),
-- Amplify Curse: the next curse, Fear or Death Coil is stronger
(18288, 0, 0, 0, 0, 0, 0, 0x15400, 0, 7, 3, 0, 8, 0, 0, 100, 0, 1),
-- Fel Domination: the next summon is faster and cheaper
(18708, 0, 0, 0, 0, 0, 0, 0x15400, 0, 7, 3, 0, 8, 0, 0, 100, 0, 1),
-- Divine Favor: the next holy spell is a critical hit
(20216, 0, 0, 0, 0, 0, 0, 0x15400, 0, 7, 3, 0, 8, 0, 0, 100, 0, 1);
