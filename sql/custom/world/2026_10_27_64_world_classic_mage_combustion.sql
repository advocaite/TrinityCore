-- Classic 1.60 (WoW Forever) mage talent Combustion (11129): a stack of 28682 per Fire damage spell hit, a charge per critical
-- strike (src/server/scripts/Custom/Classic/world/talents/classic_talents_mage_shaman.cpp)

DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_mage_combustion';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(11129, 'classic_spell_mage_combustion');

DELETE FROM `spell_proc` WHERE `SpellId` = 11129;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(11129, 4, 3, 0, 0, 0, 0, 0x10000, 0, 1, 2, 0, 0, 0, 0, 0, 0, 0); -- Combustion: the mage's Fire damage spell hits (3 charges, used by critical strikes)
