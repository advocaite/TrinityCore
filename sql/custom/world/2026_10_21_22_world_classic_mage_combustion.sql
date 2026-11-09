-- Classic 1.60 Combustion (11129, charges 3) and its critical chance buff (28682, endless, stacks): classic_spell_mage_combustion and
-- classic_spell_mage_combustion_stacks (src/server/scripts/Custom/Classic/world/talents/classic_talents_mage_shaman.cpp).
-- 11129 procs on fire critical hits (one charge each), 28682 on every fire damage hit (one stack each). Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('classic_spell_mage_combustion', 'classic_spell_mage_combustion_stacks') OR (`spell_id` IN (11129, 28682) AND `ScriptName` LIKE 'spell_mage%');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(11129, 'classic_spell_mage_combustion'),
(28682, 'classic_spell_mage_combustion_stacks');

DELETE FROM `spell_proc` WHERE `SpellId` IN (11129, 28682);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(11129, 4, 0, 0, 0, 0, 0, 0x10000, 0, 1, 2, 0x2, 0, 0, 0, 0, 0, 0), -- Combustion: fire spell critical hits, one charge each
(28682, 4, 0, 0, 0, 0, 0, 0x10000, 0, 1, 2, 0, 0, 0, 0, 0, 0, 0); -- Combustion buff: fire spell hits, one stack each
