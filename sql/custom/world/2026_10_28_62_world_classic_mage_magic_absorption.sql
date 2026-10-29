-- Classic 1.60 (WoW Forever) mage talent Magic Absorption (29441): the spells the mage fully resists restore mana
-- (src/server/scripts/Custom/Classic/world/talents/classic_talents_mage_shaman.cpp)

DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_mage_magic_absorption';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(29441, 'classic_spell_mage_magic_absorption');

DELETE FROM `spell_proc` WHERE `SpellId` = 29441;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(29441, 0, 0, 0, 0, 0, 0, 0x20000, 0, 7, 2, 0xC, 0, 0, 0, 0, 0, 0); -- Magic Absorption: harmful spells that miss the mage (vanilla: resisted) or that it resists; the client data's 1 s proc cooldown applies
