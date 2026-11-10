-- Ice Lance ranks of the trainer (1240044-1240047, 1312002) use classic_spell_mage_ice_lance like 400640 (the other Forever server package binds its
-- Ice Lance script to the same ids). Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` IN (1240044, 1240045, 1240046, 1240047, 1312002) AND `ScriptName` = 'classic_spell_mage_ice_lance';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(1240044, 'classic_spell_mage_ice_lance'),
(1240045, 'classic_spell_mage_ice_lance'),
(1240046, 'classic_spell_mage_ice_lance'),
(1240047, 'classic_spell_mage_ice_lance'),
(1312002, 'classic_spell_mage_ice_lance');
