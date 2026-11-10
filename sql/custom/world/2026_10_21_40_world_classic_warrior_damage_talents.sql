-- Classic 1.60 (WoW Forever) warrior damage talents whose client spells are dummy auras / dummy effects nothing handled.
-- C++: Custom/Classic/world/talents/classic_talents_warrior_rogue_hunter.cpp. Behaviour from VMaNGOS (GPL-2.0); amounts come from the client
-- spell data (the talent rank is the aura amount). Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('classic_spell_warr_deep_wounds', 'classic_spell_warr_deep_wound_bleed', 'classic_spell_warr_flurry',
'classic_spell_warr_unbridled_wrath', 'classic_spell_warr_sweeping_strikes', 'classic_spell_warr_bloodthirst');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(12834, 'classic_spell_warr_deep_wounds'),
(23255, 'classic_spell_warr_deep_wounds'),
(38716, 'classic_spell_warr_deep_wounds'),
(412609, 'classic_spell_warr_deep_wound_bleed'),
(12319, 'classic_spell_warr_flurry'),
(12322, 'classic_spell_warr_unbridled_wrath'),
(12292, 'classic_spell_warr_sweeping_strikes'),
(23881, 'classic_spell_warr_bloodthirst'),
(23892, 'classic_spell_warr_bloodthirst'),
(23893, 'classic_spell_warr_bloodthirst'),
(23894, 'classic_spell_warr_bloodthirst');

-- Deep Wounds and Flurry: melee critical hits of swings and melee abilities (spell type damage, hit phase, HitMask 0x2 critical).
-- Sweeping Strikes: any damaging melee swing or ability that lands (default hit mask), 5 charges from the spell.
DELETE FROM `spell_proc` WHERE `SpellId` IN (12834, 23255, 38716, 12319, 12292);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`,
`ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(12834, 0, 0, 0, 0, 0, 0, 0x14, 0, 1, 2, 0x2, 0, 0, 0, 100, 0, 0),
(23255, 0, 0, 0, 0, 0, 0, 0x14, 0, 1, 2, 0x2, 0, 0, 0, 100, 0, 0),
(38716, 0, 0, 0, 0, 0, 0, 0x14, 0, 1, 2, 0x2, 0, 0, 0, 100, 0, 0),
(12319, 0, 0, 0, 0, 0, 0, 0x14, 0, 1, 2, 0x2, 0, 0, 0, 100, 0, 0),
(12292, 0, 0, 0, 0, 0, 0, 0x14, 0, 1, 2, 0, 0, 0, 0, 100, 0, 0);
