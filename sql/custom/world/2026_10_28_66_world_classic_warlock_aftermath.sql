-- Classic 1.60 (WoW Forever) warlock talent Aftermath (18119): Conflagrate hits daze at the talent rank's chance
-- (src/server/scripts/Custom/Classic/world/talents/classic_talents_warlock_druid.cpp)

DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_talent_warl_aftermath';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(18119, 'classic_talent_warl_aftermath');

DELETE FROM `spell_proc` WHERE `SpellId` = 18119;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(18119, 0, 5, 0x200, 0x800000, 0, 0, 0x10000, 0, 1, 2, 0, 0, 0, 0, 0, 0, 0); -- Aftermath: Conflagrate hits (the script rolls the chance)
