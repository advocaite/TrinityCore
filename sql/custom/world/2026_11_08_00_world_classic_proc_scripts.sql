-- Classic 1.60 proc auras: bind the scripts of fix_classic_proc_dummies.py.
-- Weapon imbues and Grave Touch: DUMMY auras with no trigger spell.
DELETE FROM `spell_script_names` WHERE `spell_id` IN (436519, 439431, 1260189, 1260201);
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(436519, 'classic_spell_sha_flametongue_weapon'),
(439431, 'classic_spell_sha_windfury_weapon'),
(1260189, 'classic_spell_grave_touch'),
(1260201, 'classic_spell_grave_touch');

-- Talents whose proc chance is one of their effect values (trait rank) while ProcChance is 100; Nightfall has no trigger.
DELETE FROM `spell_script_names` WHERE `spell_id` IN (19407, 14156, 12322, 12298, 12311, 12289, 20150, 16164, 13754, 1224716,
  13976, 19228, 14909, 16850, 18119, 11071, 14531, 15257, 15270, 15326, 11103, 11213, 20210, 11180, 18094);
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(19407, 'classic_spell_proc_chance_from_effect'),   -- Improved Concussive Shot
(14156, 'classic_spell_proc_chance_from_effect'),   -- Ruthlessness
(12322, 'classic_spell_proc_chance_from_effect'),   -- Unbridled Wrath
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
(11103, 'classic_spell_proc_chance_from_effect'),   -- Impact
(11213, 'classic_spell_proc_chance_from_effect_dummy'), -- Arcane Concentration
(20210, 'classic_spell_proc_chance_from_effect_dummy'), -- Illumination
(11180, 'classic_spell_proc_chance_from_effect_dummy'), -- Winter's Chill
(18094, 'classic_spell_warl_nightfall');            -- Nightfall

-- Reckoning and Missile Barrage: bind the scripts of fix_classic_reckoning_barrage.py.
DELETE FROM `spell_script_names` WHERE `spell_id` IN (20177, 1326084, 400588);
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(20177, 'classic_spell_pal_reckoning'),
(1326084, 'classic_spell_pal_reckoning_attacks'),
(400588, 'classic_spell_mage_missile_barrage');

-- Reckoning's 1.5 s cooldown applies to blocks only (tooltip); the script keeps it, the aura's own cooldown is cut to 1 ms.
-- Zero in a spell_proc column means "take the spell's value", so 1 is the smallest that overrides the 1500 ms.
DELETE FROM `spell_proc` WHERE `SpellId` = 20177;
INSERT INTO `spell_proc` (`SpellId`, `Cooldown`) VALUES (20177, 1);

