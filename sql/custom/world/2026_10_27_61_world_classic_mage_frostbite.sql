-- Classic 1.60 (WoW Forever) mage talent Frostbite (11071): its chance comes from the talent rank, on Chill effects
-- (src/server/scripts/Custom/Classic/world/talents/classic_talents_mage_shaman.cpp)

DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_mage_frostbite';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(11071, 'classic_spell_mage_frostbite');

DELETE FROM `spell_proc` WHERE `SpellId` = 11071;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(11071, 0, 3, 0x100000, 0, 0, 0, 0x10000, 0, 7, 2, 0, 0, 0, 0, 0, 0, 0); -- Frostbite: the mage's Chill effects (the script rolls the chance)
