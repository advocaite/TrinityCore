-- Classic 1.60 Nightfall (18094): classic_talent_warl_nightfall was registered in C++ but never bound to the spell (no spell_script_names row in any
-- file), so the talent never cast Shadow Trance 17941. Bound here, with a proc row for what the script reads: periodic damage of Corruption
-- (warlock family flag 0x2) and Drain Life (0x8). Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_talent_warl_nightfall';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(18094, 'classic_talent_warl_nightfall');

DELETE FROM `spell_proc` WHERE `SpellId` = 18094;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`,
`ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(18094, 32, 5, 0xA, 0, 0, 0, 0x40000, 0, 1, 2, 0, 0, 0, 0, 100, 0, 0); -- periodic damage done
