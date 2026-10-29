-- Classic 1.60: Searing Light (priest talent 14909) gives Holy Purpose at its rank's chance from Holy Fire's periodic damage (family flag
-- 0x100000), and the next Holy Nova (0x400000) uses Holy Purpose up (cast phase, its spell modifier). Before, every spell gave it and
-- nothing used it (classic_talents_priest_paladin.cpp). Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` = 14909 AND `ScriptName` = 'classic_spell_pri_searing_light';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(14909, 'classic_spell_pri_searing_light');

DELETE FROM `spell_proc` WHERE `SpellId` IN (14909, 1284536);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(14909, 0, 6, 0x100000, 0, 0, 0, 0x40000, 0, 0x1, 0x2, 0, 0, 0x1, 0, 100, 0, 0),
(1284536, 0, 6, 0x400000, 0, 0, 0, 0x14000, 0, 0x7, 0x1, 0, 0x8, 0, 0, 100, 0, 0);
