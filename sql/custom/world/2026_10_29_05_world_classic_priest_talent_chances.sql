-- Classic 1.60: priest talents whose chance is their rank value proc at that chance, from what their tooltips name
-- (classic_talents_priest_paladin.cpp):
--   15326 Blackout: Shadow damage spells and their periodic damage. Without a row it had no proc data at all (it can proc from procs)
--   15257 Shadow Weaving: Shadow spells. The same: no proc data without a row
--   14531 Martyrdom: melee and ranged critical strikes taken (the client data: any hit taken)
--   15270 Spirit Tap: kills that give experience or honor, as the client data
-- Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` IN (15326, 15257, 14531, 15270) AND `ScriptName` = 'classic_spell_pri_talent_chance';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(15326, 'classic_spell_pri_talent_chance'),
(15257, 'classic_spell_pri_talent_chance'),
(14531, 'classic_spell_pri_talent_chance'),
(15270, 'classic_spell_pri_talent_chance');

DELETE FROM `spell_proc` WHERE `SpellId` IN (15326, 15257, 14531, 15270);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(15326, 0x20, 0, 0, 0, 0, 0, 0x50000, 0, 0x1, 0x2, 0, 0, 0, 0, 100, 0, 0),
(15257, 0x20, 0, 0, 0, 0, 0, 0x10000, 0, 0x5, 0x2, 0, 0, 0, 0, 100, 0, 0),
(14531, 0, 0, 0, 0, 0, 0, 0x2A8, 0, 0, 0, 0x2, 0, 0, 0, 100, 0, 0),
(15270, 0, 0, 0, 0, 0, 0, 0x2, 0, 0, 0, 0, 0x1, 0, 0, 100, 0, 0);
