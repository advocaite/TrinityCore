-- Proc rows for nine Classic 1.60 talent spells and talent buffs that had none (data check 2026-11-12). Written from the client
-- spell data (build 70532/70888): each ability's class flags and damage class, the buffs' own spell modifiers. Without a row the
-- server built one from the client's proc data: for the four "chance to ..." talents that is every melee, ranged and spell hit
-- at 100%, and Missile Barrage's buff has no client proc data at all, so it was never used up. Safe to run again.
--  12289 Improved Hamstring      Hamstring (warrior 0x2), melee ability hit; chance = talent amount (script)
--  12311 Improved Shield Bash    Shield Bash (warrior 0x800), melee ability hit; chance = talent amount (script)
--  19228 Improved Wing Clip      Wing Clip (hunter 0x40), melee ability hit; chance = talent amount (script)
--  19407 Improved Concussive Shot Concussive Shot (hunter 0x200), ranged ability hit; chance = talent amount (script)
--  16880 Nature's Grace          non-periodic damage or heal spell critical hits
--  17079, 17082 Improved Enrage  Enrage (druid 0x80000): the rage spell (17080 / 17081) on each Enrage
--  16246 Clearcasting (shaman)   used up by the next spell its mana cost modifier changes (Lightning Bolt, Chain Lightning, shocks,
--                                Fire Nova, Lava Burst: the modifier's own class mask), on cast
--  400589 Missile Barrage (buff) used up by the next Arcane Missiles its duration and cost modifiers change, on cast
-- Charges is 1 on both buffs: a row's Charges replaces the client's charge count (Aura::CalcMaxCharges), 0 would make them last.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_talent_ability_proc_chance';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(12289, 'classic_spell_talent_ability_proc_chance'),
(12311, 'classic_spell_talent_ability_proc_chance'),
(19228, 'classic_spell_talent_ability_proc_chance'),
(19407, 'classic_spell_talent_ability_proc_chance');

DELETE FROM `spell_proc` WHERE `SpellId` IN (12289, 12311, 19228, 19407, 16880, 17079, 17082, 16246, 400589);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`,
`ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(12289,  0, 4,  0x2,        0,      0, 0, 0x10,    0, 0, 2, 0,   0,   0, 0, 100, 0, 0),
(12311,  0, 4,  0x800,      0,      0, 0, 0x10,    0, 0, 2, 0,   0,   0, 0, 100, 0, 0),
(19228,  0, 9,  0x40,       0,      0, 0, 0x10,    0, 0, 2, 0,   0,   0, 0, 100, 0, 0),
(19407,  0, 9,  0x200,      0,      0, 0, 0x100,   0, 0, 2, 0,   0,   0, 0, 100, 0, 0),
(16880,  0, 7,  0,          0,      0, 0, 0x14000, 0, 3, 2, 0x2, 0,   0, 0, 100, 0, 0),
(17079,  0, 7,  0x80000,    0,      0, 0, 0x400,   0, 0, 2, 0,   0,   0, 0, 100, 0, 0),
(17082,  0, 7,  0x80000,    0,      0, 0, 0x400,   0, 0, 2, 0,   0,   0, 0, 100, 0, 0),
(16246,  0, 11, 0x98100003, 0x1000, 0, 0, 0x11000, 0, 1, 1, 0,   0x8, 0, 0, 100, 0, 1),
(400589, 0, 3,  0x800,      0,      0, 0, 0x11000, 0, 0, 1, 0,   0x8, 0, 0, 100, 0, 1);
