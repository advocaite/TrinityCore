-- Classic 1.60 Arcane Concentration (11213): a proc trigger of Clearcasting 12536 with 100 as spell proc chance; the talent chance is the effect amount.
-- classic_spell_mage_arcane_concentration rolls that amount, the row limits it to damage spell hits. Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_mage_arcane_concentration';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(11213, 'classic_spell_mage_arcane_concentration');

DELETE FROM `spell_proc` WHERE `SpellId` = 11213;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`,
`ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(11213, 0, 0, 0, 0, 0, 0, 0x10000, 0, 1, 2, 0, 0, 0, 0, 100, 0, 0); -- harmful spell damage hits
