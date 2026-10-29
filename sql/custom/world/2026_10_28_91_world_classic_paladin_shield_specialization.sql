-- Classic 1.60: the paladin's Shield Specialization (20150) restores mana from blocks only, at its rank's chance, once every 3 sec
-- (classic_talents_priest_paladin.cpp). Its client proc data had no hit mask and a 100% chance. Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` = 20150 AND `ScriptName` = 'classic_spell_pal_shield_specialization';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(20150, 'classic_spell_pal_shield_specialization');

DELETE FROM `spell_proc` WHERE `SpellId` = 20150;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(20150, 0, 0, 0, 0, 0, 0, 0x2A8, 0, 0, 0, 0x2040, 0, 0x1, 0, 100, 3000, 0);
