-- Classic 1.60: Reckoning (paladin talent) and Fast Recovery (Legacy perk) are dummy auras in the 70532 data that nothing handled.
-- C++: classic_talents_priest_paladin.cpp (classic_spell_pal_reckoning, classic_spell_pal_reckoning_stored) and classic_spell_scripts.cpp
-- (classic_spell_legacy_fast_recovery). Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('classic_spell_pal_reckoning', 'classic_spell_pal_reckoning_stored', 'classic_spell_legacy_fast_recovery');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(20177, 'classic_spell_pal_reckoning'),
(1326084, 'classic_spell_pal_reckoning_stored'),
(1225505, 'classic_spell_legacy_fast_recovery');

-- Reckoning: a block or a critical hit taken (2026_10_21_41 had critical hits only; the 70532 tooltip adds blocks). The chance is rolled by
-- the script ($m1% after a block, 2.5 x that after a critical hit); ProcFlags, Chance and Cooldown 0 keep the client's (1.5 sec proc cooldown).
-- The stored attacks (1326084) are used by the next melee swing, whatever its outcome (hit, crit, miss, dodge, parry, block, absorb).
DELETE FROM `spell_proc` WHERE `SpellId` IN (20177, 1326084);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`,
`ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(20177, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x42, 0, 0, 0, 100, 0, 0),
(1326084, 0, 0, 0, 0, 0, 0, 0x4, 0, 0, 0, 0x477, 0, 0, 0, 100, 0, 0);
