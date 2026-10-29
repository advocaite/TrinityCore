-- Classic 1.60 (WoW Forever) mage talent Master of Elements (29074): Fire and Frost critical strikes refund mana
-- (src/server/scripts/Custom/Classic/world/talents/classic_talents_mage_shaman.cpp)

DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_mage_master_of_elements';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(29074, 'classic_spell_mage_master_of_elements');

DELETE FROM `spell_proc` WHERE `SpellId` = 29074;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(29074, 20, 0, 0, 0, 0, 0, 0x10000, 0, 1, 2, 0x2, 0, 0, 0, 0, 0, 0); -- Master of Elements: Fire and Frost spell critical hits
