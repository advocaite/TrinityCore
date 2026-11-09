-- Classic 1.60 (WoW Forever) warrior Last Stand and Retaliation: the client spells are a dummy effect / dummy aura that nothing handled.
-- C++: Custom/Classic/world/talents/classic_talents_warrior_rogue_hunter.cpp. Behaviour from VMaNGOS (GPL-2.0). Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('classic_spell_warr_last_stand', 'classic_spell_warr_retaliation');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(12975, 'classic_spell_warr_last_stand'),
(20230, 'classic_spell_warr_retaliation');

-- Retaliation: every melee swing taken from the front (30 charges and 15 s come from the spell).
DELETE FROM `spell_proc` WHERE `SpellId` = 20230;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`,
`ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(20230, 0, 0, 0, 0, 0, 0, 0x8, 0, 0, 0, 0, 0, 0, 0, 100, 0, 0);
