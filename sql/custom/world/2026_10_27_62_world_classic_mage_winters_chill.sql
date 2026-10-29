-- Classic 1.60 (WoW Forever) mage talent Winter's Chill (11180): one stack per proc, its chance from the talent rank, on Frost
-- damage spell hits (src/server/scripts/Custom/Classic/world/talents/classic_talents_mage_shaman.cpp)

DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_mage_winters_chill';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(11180, 'classic_spell_mage_winters_chill');

DELETE FROM `spell_proc` WHERE `SpellId` = 11180;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(11180, 16, 3, 0, 0, 0, 0, 0x10000, 0, 1, 2, 0, 0, 0, 0, 0, 0, 0); -- Winter's Chill: the mage's Frost damage spell hits (the script rolls the chance)
