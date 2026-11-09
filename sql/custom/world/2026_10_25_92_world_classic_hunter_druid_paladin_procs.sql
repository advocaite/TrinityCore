-- Classic 1.60: more 70532 dummy effects with no handler, and Holy Shield's proc. C++: classic_talents_warrior_rogue_hunter.cpp
-- (classic_spell_hun_summon_hawk; Rapid Killing is a UnitScript) and classic_talents_warlock_druid.cpp (classic_talent_dru_natural_reaction).
-- Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('classic_spell_hun_summon_hawk', 'classic_talent_dru_natural_reaction');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
-- Summon Hawk: the dive adds $s4% of ranged attack power
(1293525, 'classic_spell_hun_summon_hawk'),
(1293526, 'classic_spell_hun_summon_hawk'),
(1293527, 'classic_spell_hun_summon_hawk'),
(1293241, 'classic_spell_hun_summon_hawk'),
-- Natural Reaction: rage on dodge
(417051, 'classic_talent_dru_natural_reaction');

-- Natural Reaction: the talent has no proc data; a dodged melee swing or melee ability rolls the chance in the script.
-- Holy Shield (20925, 20927, 20928): "deals $s2 Holy damage for each attack blocked". Without a row the core takes the client proc data with
-- the default taken hit mask (normal and critical hits), so every hit used a charge and dealt the damage, blocked or not. Blocks only, as in
-- VMaNGOS (spell_proc_event procEx BLOCK). ProcFlags, Chance, Cooldown and Charges 0 keep the client's.
DELETE FROM `spell_proc` WHERE `SpellId` IN (417051, 20925, 20927, 20928);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`,
`ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(417051, 0, 0, 0, 0, 0, 0, 0x28, 0, 0, 0, 0x10, 0, 0, 0, 100, 0, 0),
(20925, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x40, 0, 0, 0, 0, 0, 0),
(20927, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x40, 0, 0, 0, 0, 0, 0),
(20928, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x40, 0, 0, 0, 0, 0, 0);
