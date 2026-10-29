-- Classic 1.60 (WoW Forever) mage talent Arcane Blast (400574 and its ranks): each cast adds a stack of 400573 (Intellect as spell
-- damage, a higher Arcane Blast mana cost) (src/server/scripts/Custom/Classic/world/talents/classic_talents_mage_shaman.cpp)

DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_mage_arcane_blast';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(400574, 'classic_spell_mage_arcane_blast'),
(1239696, 'classic_spell_mage_arcane_blast'),
(1239697, 'classic_spell_mage_arcane_blast'),
(1239699, 'classic_spell_mage_arcane_blast'),
(1239700, 'classic_spell_mage_arcane_blast');
