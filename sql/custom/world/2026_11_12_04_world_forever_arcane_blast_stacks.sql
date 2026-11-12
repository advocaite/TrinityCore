-- Arcane Blast (all ranks) gives its stacking buff 400573 (Discord 2026-11-12, pennywhi: "Arcane Blast doesn't stack").
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_mage_arcane_blast';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(400574, 'classic_spell_mage_arcane_blast'),
(1239696, 'classic_spell_mage_arcane_blast'),
(1239697, 'classic_spell_mage_arcane_blast'),
(1239699, 'classic_spell_mage_arcane_blast'),
(1239700, 'classic_spell_mage_arcane_blast');
