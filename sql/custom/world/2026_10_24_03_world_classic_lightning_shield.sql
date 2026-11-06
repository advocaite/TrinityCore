-- All Lightning Shield ranks trigger dummy spell 26545 (SPELL_EFFECT_DUMMY, no script), so attackers took no
-- damage. classic_spell_sha_lightning_shield casts the rank's damage spell instead (26364..26370, 26363).
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_sha_lightning_shield';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(324, 'classic_spell_sha_lightning_shield'),
(325, 'classic_spell_sha_lightning_shield'),
(905, 'classic_spell_sha_lightning_shield'),
(945, 'classic_spell_sha_lightning_shield'),
(8134, 'classic_spell_sha_lightning_shield'),
(10431, 'classic_spell_sha_lightning_shield'),
(10432, 'classic_spell_sha_lightning_shield');
