-- Classic 1.60: Flurry (warrior talent 12319) gives attack speed for 3 swings after a melee critical strike
-- (classic_talents_warrior_rogue_hunter.cpp). Its client proc data names spells and ranged attacks too: only melee critical strikes.
-- Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` = 12319 AND `ScriptName` = 'classic_spell_warr_flurry';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(12319, 'classic_spell_warr_flurry');

DELETE FROM `spell_proc` WHERE `SpellId` = 12319;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(12319, 0, 0, 0, 0, 0, 0, 0x14, 0, 1, 2, 0x2, 0, 0, 0, 100, 0, 0);
