-- Classic 1.60 Master of the Elements (29074): classic_spell_mage_master_of_elements gives back the amount percent of the mana a Fire or
-- Frost spell cost when it critically hits (direct damage). Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_mage_master_of_elements' OR (`spell_id` = 29074 AND `ScriptName` LIKE 'spell_mage%');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(29074, 'classic_spell_mage_master_of_elements');

DELETE FROM `spell_proc` WHERE `SpellId` = 29074;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(29074, 20, 0, 0, 0, 0, 0, 0x10000, 0, 1, 2, 0x2, 0, 0, 0, 0, 0, 0); -- Fire (4) and Frost (16) spell critical hits
