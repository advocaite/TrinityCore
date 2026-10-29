-- Classic 1.60 (WoW Forever) Ice Lance: increased damage to Frozen targets
-- (src/server/scripts/Custom/Classic/world/talents/classic_talents_mage_shaman.cpp)

DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_mage_ice_lance';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(1312002, 'classic_spell_mage_ice_lance'),
(400640, 'classic_spell_mage_ice_lance'),
(1240044, 'classic_spell_mage_ice_lance'),
(1240045, 'classic_spell_mage_ice_lance'),
(1240046, 'classic_spell_mage_ice_lance'),
(1240047, 'classic_spell_mage_ice_lance');
