-- Classic 1.60: Rupture and Garrote grow with attack power (patch 1.12, behaviour from vMaNGOS Aura::CalculateDotDamage).
-- C++: Custom/Classic/world/talents/classic_rogue_abilities.cpp (classic_spell_rog_bleed_attack_power). Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_rog_bleed_attack_power';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(1943,'classic_spell_rog_bleed_attack_power'),(8639,'classic_spell_rog_bleed_attack_power'),(8640,'classic_spell_rog_bleed_attack_power'),
(11273,'classic_spell_rog_bleed_attack_power'),(11274,'classic_spell_rog_bleed_attack_power'),(11275,'classic_spell_rog_bleed_attack_power'),
(703,'classic_spell_rog_bleed_attack_power'),(8631,'classic_spell_rog_bleed_attack_power'),(8632,'classic_spell_rog_bleed_attack_power'),
(8633,'classic_spell_rog_bleed_attack_power'),(11289,'classic_spell_rog_bleed_attack_power'),(11290,'classic_spell_rog_bleed_attack_power');
