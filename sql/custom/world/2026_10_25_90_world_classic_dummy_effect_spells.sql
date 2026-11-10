-- Classic 1.60: trainer spells and talents whose 70532 effect is a dummy (or a script effect) that no handler read, so the tooltip effect
-- never happened. C++: Custom/Classic/world/talents/*.cpp and classic_spells_priest.cpp. Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('classic_spell_sha_lava_burst', 'classic_spell_sha_chain_heal_riptide',
    'classic_spell_mage_arcane_wrath', 'classic_spell_warl_incinerate', 'classic_talent_dru_feral_swiftness',
    'classic_spell_warr_spearing_strike', 'classic_spell_rog_mutilate_strike', 'classic_spell_rog_ghostly_strike', 'classic_spell_pri_divine_grace');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
-- Lava Burst: +$s2% damage with the shaman's Flame Shock on the target (was: always a critical strike); 1326047 is the level 60 rank
(408490, 'classic_spell_sha_lava_burst'),
(1238299, 'classic_spell_sha_lava_burst'),
(1238300, 'classic_spell_sha_lava_burst'),
(1326047, 'classic_spell_sha_lava_burst'),
-- Chain Heal: +$s3% on the first target with the shaman's Riptide
(1064, 'classic_spell_sha_chain_heal_riptide'),
(10622, 'classic_spell_sha_chain_heal_riptide'),
(10623, 'classic_spell_sha_chain_heal_riptide'),
-- Arcane Wrath: extra spell damage of $s2% of maximum mana
(1326080, 'classic_spell_mage_arcane_wrath'),
(1326081, 'classic_spell_mage_arcane_wrath'),
(1326082, 'classic_spell_mage_arcane_wrath'),
-- Incinerate: +$m2% with the warlock's Immolate on the target
(412758, 'classic_spell_warl_incinerate'),
(1293812, 'classic_spell_warl_incinerate'),
(1293813, 'classic_spell_warl_incinerate'),
-- Feral Swiftness: Cat Form movement speed
(17002, 'classic_talent_dru_feral_swiftness'),
-- Spearing Strike: more weapon damage against Giants, Dragonkin and mounted targets, and the dismount
(1310222, 'classic_spell_warr_spearing_strike'),
-- Mutilate strikes: +$m4% against poisoned targets
(399960, 'classic_spell_rog_mutilate_strike'),
(399961, 'classic_spell_rog_mutilate_strike'),
(1241585, 'classic_spell_rog_mutilate_strike'),
(1241588, 'classic_spell_rog_mutilate_strike'),
(1241586, 'classic_spell_rog_mutilate_strike'),
(1241590, 'classic_spell_rog_mutilate_strike'),
(1310705, 'classic_spell_rog_mutilate_strike'),
(1310706, 'classic_spell_rog_mutilate_strike'),
-- Ghostly Strike: the dagger weapon damage percent
(14278, 'classic_spell_rog_ghostly_strike'),
-- Divine Grace: only below $s2% health, removes Weakened Soul
(1277370, 'classic_spell_pri_divine_grace'),
(1277371, 'classic_spell_pri_divine_grace'),
(1277372, 'classic_spell_pri_divine_grace'),
(1277374, 'classic_spell_pri_divine_grace'),
(1277376, 'classic_spell_pri_divine_grace'),
(1277377, 'classic_spell_pri_divine_grace'),
(1277378, 'classic_spell_pri_divine_grace');
