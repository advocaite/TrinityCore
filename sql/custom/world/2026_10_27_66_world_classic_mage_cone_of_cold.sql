-- Classic 1.60 (WoW Forever) Cone of Cold: the first targets hit are slowed more and longer
-- (src/server/scripts/Custom/Classic/world/classic_spell_scripts.cpp)

DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_mage_cone_of_cold';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(120, 'classic_spell_mage_cone_of_cold'),
(8492, 'classic_spell_mage_cone_of_cold'),
(10159, 'classic_spell_mage_cone_of_cold'),
(10160, 'classic_spell_mage_cone_of_cold'),
(10161, 'classic_spell_mage_cone_of_cold');
