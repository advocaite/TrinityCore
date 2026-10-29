-- Classic 1.60: warrior talents whose chance is their rank value proc at that chance, from the spells and hits their tooltips name
-- (classic_talents_warrior_rogue_hunter.cpp). Their client proc chance was 100%, from any melee attack:
--   12322 Unbridled Wrath: melee damage with a weapon (swings, as the client data)
--   12289 Improved Hamstring: Hamstring (family flag 0x2)
--   12311 Improved Shield Bash: Shield Bash (0x800)
--   1310316 Master of Defense: dodging or parrying while a shield is equipped
--   12298 Shield Specialization (effect 1): blocking
-- Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` IN (12322, 12289, 12311, 1310316, 12298) AND `ScriptName` IN ('classic_spell_warr_talent_chance', 'classic_spell_warr_shield_specialization', 'classic_spell_warr_master_of_defense');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(12322, 'classic_spell_warr_talent_chance'),
(12289, 'classic_spell_warr_talent_chance'),
(12311, 'classic_spell_warr_talent_chance'),
(1310316, 'classic_spell_warr_master_of_defense'),
(12298, 'classic_spell_warr_shield_specialization');

DELETE FROM `spell_proc` WHERE `SpellId` IN (12322, 12289, 12311, 1310316, 12298);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(12322, 0, 0, 0, 0, 0, 0, 0x4, 0, 0, 0, 0, 0, 0, 0, 100, 0, 0),
(12289, 0, 4, 0x2, 0, 0, 0, 0x10, 0, 1, 2, 0, 0, 0, 0, 100, 0, 0),
(12311, 0, 4, 0x800, 0, 0, 0, 0x10, 0, 1, 2, 0, 0, 0, 0, 100, 0, 0),
(1310316, 0, 0, 0, 0, 0, 0, 0x2A8, 0, 0, 0, 0x30, 0, 0, 0, 100, 0, 0),
(12298, 0, 0, 0, 0, 0, 0, 0x2A8, 0, 0, 0, 0x2040, 0, 0x1, 0, 100, 0, 0);
