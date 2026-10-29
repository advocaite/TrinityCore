-- Classic 1.60: rogue talents whose chance is their rank value proc at that chance, from the spells their tooltips name
-- (classic_talents_warrior_rogue_hunter.cpp). Their client proc chance was 100%, from any melee ability:
--   13754 Improved Kick: Kick (family flag 0x10)
--   13976 Initiative: Ambush, Garrote and Cheap Shot (0x200, 0x100, 0x400)
--   14156 Ruthlessness: finishing moves (0x3E0000, and 0x4000004 in the 3rd mask, as Relentless Strikes), with the client's 500 ms
--   cooldown
-- Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` IN (13754, 13976, 14156) AND `ScriptName` = 'classic_spell_rog_talent_chance';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(13754, 'classic_spell_rog_talent_chance'),
(13976, 'classic_spell_rog_talent_chance'),
(14156, 'classic_spell_rog_talent_chance');

DELETE FROM `spell_proc` WHERE `SpellId` IN (13754, 13976, 14156);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(13754, 0, 8, 0x10, 0, 0, 0, 0x10, 0, 0x7, 0x2, 0, 0, 0, 0, 100, 0, 0),
(13976, 0, 8, 0x700, 0, 0, 0, 0x10, 0, 0x7, 0x2, 0, 0, 0, 0, 100, 0, 0),
(14156, 0, 8, 0x3E0000, 0, 0x4000004, 0, 0x10, 0, 0x7, 0x2, 0, 0, 0, 0, 100, 500, 0);
