-- Flametongue Weapon added no Fire damage: in 1.60 every rank's enchantment applies the dummy proc aura 436519, which had no
-- script. classic_spell_sha_flametongue_weapon deals the rank's damage (from the enchantment on the weapon that hit).
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_sha_flametongue_weapon';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(436519, 'classic_spell_sha_flametongue_weapon');
