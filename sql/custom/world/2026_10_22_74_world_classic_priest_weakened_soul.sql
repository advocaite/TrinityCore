-- Classic 1.60 (WoW Forever) priest Power Word: Shield: the target gets Weakened Soul and refuses another shield while it has it. The retail script
-- that did this never attached (Validate failed in the 948952c1 start log); the Classic script in classic_spells_priest.cpp existed but nothing bound it.
-- Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_pri_power_word_shield';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(17, 'classic_spell_pri_power_word_shield'),
(592, 'classic_spell_pri_power_word_shield'),
(600, 'classic_spell_pri_power_word_shield'),
(3747, 'classic_spell_pri_power_word_shield'),
(6065, 'classic_spell_pri_power_word_shield'),
(6066, 'classic_spell_pri_power_word_shield'),
(10898, 'classic_spell_pri_power_word_shield'),
(10899, 'classic_spell_pri_power_word_shield'),
(10900, 'classic_spell_pri_power_word_shield'),
(10901, 'classic_spell_pri_power_word_shield');
