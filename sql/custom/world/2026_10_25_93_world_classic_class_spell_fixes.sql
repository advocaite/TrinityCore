-- Classic 1.60: talents and trainer spells whose 70532 tooltip part had no handler. C++: classic_talents_priest_paladin.cpp,
-- classic_talents_warlock_druid.cpp, classic_talents_warrior_rogue_hunter.cpp, classic_talents_mage_shaman.cpp (Devouring Contagion is a
-- UnitScript). Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('classic_spell_pal_guarded_by_the_light', 'classic_spell_pal_guarded_by_the_light_cancel',
'classic_spell_pal_holy_strike', 'classic_spell_pri_mind_flay', 'classic_talent_warl_decimation', 'classic_spell_warl_decimation',
'classic_spell_dru_entangling_roots_limit', 'classic_spell_hun_expose_prey', 'classic_spell_hun_mongoose_bite',
'classic_spell_hun_mend_pet', 'classic_spell_mage_missile_barrage');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
-- Guarded by the Light: mana 415058 after a block; Flash of Light, Holy Light and Holy Shock cancel it
(415059, 'classic_spell_pal_guarded_by_the_light'),
(19750, 'classic_spell_pal_guarded_by_the_light_cancel'),
(19939, 'classic_spell_pal_guarded_by_the_light_cancel'),
(19940, 'classic_spell_pal_guarded_by_the_light_cancel'),
(19941, 'classic_spell_pal_guarded_by_the_light_cancel'),
(19942, 'classic_spell_pal_guarded_by_the_light_cancel'),
(19943, 'classic_spell_pal_guarded_by_the_light_cancel'),
(635, 'classic_spell_pal_guarded_by_the_light_cancel'),
(639, 'classic_spell_pal_guarded_by_the_light_cancel'),
(647, 'classic_spell_pal_guarded_by_the_light_cancel'),
(1026, 'classic_spell_pal_guarded_by_the_light_cancel'),
(1042, 'classic_spell_pal_guarded_by_the_light_cancel'),
(3472, 'classic_spell_pal_guarded_by_the_light_cancel'),
(10328, 'classic_spell_pal_guarded_by_the_light_cancel'),
(10329, 'classic_spell_pal_guarded_by_the_light_cancel'),
(25292, 'classic_spell_pal_guarded_by_the_light_cancel'),
(20473, 'classic_spell_pal_guarded_by_the_light_cancel'),
(20929, 'classic_spell_pal_guarded_by_the_light_cancel'),
(20930, 'classic_spell_pal_guarded_by_the_light_cancel'),
(1311606, 'classic_spell_pal_guarded_by_the_light_cancel'),
-- Holy Strike: the whole flat $s1 (it was halved by the weapon percent) plus its spell power share
(679, 'classic_spell_pal_holy_strike'),
(678, 'classic_spell_pal_holy_strike'),
(1866, 'classic_spell_pal_holy_strike'),
(680, 'classic_spell_pal_holy_strike'),
(2495, 'classic_spell_pal_holy_strike'),
(5569, 'classic_spell_pal_holy_strike'),
(10332, 'classic_spell_pal_holy_strike'),
(10333, 'classic_spell_pal_holy_strike'),
-- Mind Flay with Improved Mind Flay: each tick $s1% more than the one before
(15407, 'classic_spell_pri_mind_flay'),
(17311, 'classic_spell_pri_mind_flay'),
(17312, 'classic_spell_pri_mind_flay'),
(17313, 'classic_spell_pri_mind_flay'),
(17314, 'classic_spell_pri_mind_flay'),
(18807, 'classic_spell_pri_mind_flay'),
-- Decimation: the damage bonus only below the health threshold, Soul Fire buff 440873 from Shadow Bolt and Searing Pain there
(440870, 'classic_talent_warl_decimation'),
(686, 'classic_spell_warl_decimation'),
(695, 'classic_spell_warl_decimation'),
(705, 'classic_spell_warl_decimation'),
(1088, 'classic_spell_warl_decimation'),
(1106, 'classic_spell_warl_decimation'),
(7641, 'classic_spell_warl_decimation'),
(11659, 'classic_spell_warl_decimation'),
(11660, 'classic_spell_warl_decimation'),
(11661, 'classic_spell_warl_decimation'),
(25307, 'classic_spell_warl_decimation'),
(5676, 'classic_spell_warl_decimation'),
(17919, 'classic_spell_warl_decimation'),
(17920, 'classic_spell_warl_decimation'),
(17921, 'classic_spell_warl_decimation'),
(17922, 'classic_spell_warl_decimation'),
(17923, 'classic_spell_warl_decimation'),
-- Entangling Roots: 1 rooted target at a time, +$s1 with Overgrowth
(339, 'classic_spell_dru_entangling_roots_limit'),
(1062, 'classic_spell_dru_entangling_roots_limit'),
(5195, 'classic_spell_dru_entangling_roots_limit'),
(5196, 'classic_spell_dru_entangling_roots_limit'),
(9852, 'classic_spell_dru_entangling_roots_limit'),
(9853, 'classic_spell_dru_entangling_roots_limit'),
-- Expose Prey: only against Hunter's Mark, with the talent's chance; Mongoose Bite uses the activation
(1310532, 'classic_spell_hun_expose_prey'),
(1495, 'classic_spell_hun_mongoose_bite'),
(14269, 'classic_spell_hun_mongoose_bite'),
(14270, 'classic_spell_hun_mongoose_bite'),
(14271, 'classic_spell_hun_mongoose_bite'),
-- Mend Pet with Improved Mend Pet: cleanse chance per heal tick
(136, 'classic_spell_hun_mend_pet'),
(3111, 'classic_spell_hun_mend_pet'),
(3661, 'classic_spell_hun_mend_pet'),
(3662, 'classic_spell_hun_mend_pet'),
(13542, 'classic_spell_hun_mend_pet'),
(13543, 'classic_spell_hun_mend_pet'),
(13544, 'classic_spell_hun_mend_pet'),
-- Missile Barrage: the chance per spell
(400588, 'classic_spell_mage_missile_barrage');

-- The retail script on Entangling Roots rank 1 cleared the target of effect 1 (the Nature damage over time) unless the caster had the
-- retail talent Curious Bramblepatch, so rank 1 did no damage. The other ranks never had it.
DELETE FROM `spell_script_names` WHERE `spell_id` = 339 AND `ScriptName` = 'spell_dru_entangling_roots';

-- Guarded by the Light: client proc data is taken melee hits at 100%; blocks only.
-- Missile Barrage: chance 100 (the script rolls the per-spell chance, the client's flat 50 would halve it) and triggered spells may proc
-- (the Arcane Missiles hits are triggered by the channel).
DELETE FROM `spell_proc` WHERE `SpellId` IN (415059, 400588);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`,
`ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(415059, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x40, 0, 0, 0, 0, 0, 0),
(400588, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x2, 0, 0, 100, 0, 0);
