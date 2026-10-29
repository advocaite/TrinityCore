-- Classic 1.60 (WoW Forever) mage talent Impact (11103): its chance comes from the talent rank, on Fire spell hits
-- (src/server/scripts/Custom/Classic/world/talents/classic_talents_mage_shaman.cpp)

DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_mage_impact';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(11103, 'classic_spell_mage_impact');

DELETE FROM `spell_proc` WHERE `SpellId` = 11103;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(11103, 4, 3, 0, 0, 0, 0, 0x10000, 0, 1, 2, 0, 0, 0, 0, 0, 0, 0); -- Impact: the mage's Fire spell hits (the script rolls the chance)
