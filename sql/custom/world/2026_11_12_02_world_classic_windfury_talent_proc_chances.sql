-- Classic 1.60: Windfury Weapon procs, and talents whose proc chance is an effect value scaled by the talent rank (SpellAuraOptions
-- has 100%, so they fired on every hit). From PR #325 by MShkolenko; the spells this core already scripts its own way (Flametongue,
-- Touch of the Grave, Nightfall, Reckoning, Missile Barrage, Ruthlessness, Unbridled Wrath, Arcane Concentration, Illumination,
-- Winter's Chill) keep their scripts. C++: Custom/Classic/world/classic_spell_scripts.cpp. Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('classic_spell_sha_windfury_weapon', 'classic_spell_proc_chance_from_effect');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(439431, 'classic_spell_sha_windfury_weapon'),
(19407, 'classic_spell_proc_chance_from_effect'),   -- Improved Concussive Shot
(12298, 'classic_spell_proc_chance_from_effect'),   -- Shield Specialization (warrior)
(12311, 'classic_spell_proc_chance_from_effect'),   -- Improved Shield Bash
(12289, 'classic_spell_proc_chance_from_effect'),   -- Improved Hamstring
(20150, 'classic_spell_proc_chance_from_effect'),   -- Shield Specialization (paladin)
(16164, 'classic_spell_proc_chance_from_effect'),   -- Elemental Focus
(13754, 'classic_spell_proc_chance_from_effect'),   -- Improved Kick
(1224716, 'classic_spell_proc_chance_from_effect'), -- Puncturing Wounds
(13976, 'classic_spell_proc_chance_from_effect'),   -- Initiative
(19228, 'classic_spell_proc_chance_from_effect'),   -- Improved Wing Clip
(14909, 'classic_spell_proc_chance_from_effect'),   -- Searing Light
(16850, 'classic_spell_proc_chance_from_effect'),   -- Improved Starfire
(18119, 'classic_spell_proc_chance_from_effect'),   -- Aftermath
(11071, 'classic_spell_proc_chance_from_effect'),   -- Frostbite
(14531, 'classic_spell_proc_chance_from_effect'),   -- Martyrdom
(15257, 'classic_spell_proc_chance_from_effect'),   -- Shadow Weaving
(15270, 'classic_spell_proc_chance_from_effect'),   -- Spirit Tap
(15326, 'classic_spell_proc_chance_from_effect'),   -- Blackout
(11103, 'classic_spell_proc_chance_from_effect');   -- Impact
