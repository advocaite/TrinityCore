-- Explicit proc rows for two warlock scripts that read the proc event, so they do not depend on the client's proc flags (the mage and shaman talent
-- file has a row for each of its procs, the warlock file had none). Improved Shadow Bolt 17793: Shadow Bolt critical hits (family flag 0x1).
-- Demonic Energies 1225214: spell and periodic damage done. Safe to run again.
DELETE FROM `spell_proc` WHERE `SpellId` IN (17793, 1225214);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`,
`ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(17793, 32, 5, 0x1, 0, 0, 0, 0x10000, 0, 1, 2, 0x2, 0, 0, 0, 100, 0, 0),
(1225214, 0, 0, 0, 0, 0, 0, 0x50000, 0, 1, 2, 0, 0, 0, 0, 100, 0, 0);
