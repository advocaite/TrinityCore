-- Classic 1.60 (WoW Forever) warlock talent Nightfall (18094): the damage of Corruption, Drain Soul, Drain Life and Wrack can give a
-- Shadow Trance (17941), which the next Shadow Bolt uses up (src/server/scripts/Custom/Classic/world/talents/classic_talents_warlock_druid.cpp)

DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_talent_warl_nightfall';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(18094, 'classic_talent_warl_nightfall');

DELETE FROM `spell_proc` WHERE `SpellId` IN (18094, 17941);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(18094, 0, 5, 0x400A, 0x40000, 0, 0, 0x40000, 0, 1, 2, 0, 0, 0, 0, 0, 0, 0), -- Nightfall: damage over time ticks of Corruption, Drain Life, Drain Soul and Wrack (the script rolls the chance)
(17941, 0, 5, 0x1, 0, 0, 0, 0x10000, 0, 7, 1, 0, 8, 0, 0, 100, 0, 1); -- Shadow Trance: used by the next Shadow Bolt whose cast time it removes (no proc chance in the client data)
