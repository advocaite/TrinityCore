-- Classic 1.60 (WoW Forever) mage spell Arcane Wrath (1326079-1326082): a share of the mage's maximum mana adds to its spell damage
-- (src/server/scripts/Custom/Classic/world/talents/classic_talents_mage_shaman.cpp)

DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_mage_arcane_wrath';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(1326079, 'classic_spell_mage_arcane_wrath'),
(1326080, 'classic_spell_mage_arcane_wrath'),
(1326081, 'classic_spell_mage_arcane_wrath'),
(1326082, 'classic_spell_mage_arcane_wrath');
