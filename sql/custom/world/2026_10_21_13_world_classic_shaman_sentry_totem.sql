-- Classic 1.60: Sentry Totem lets the shaman see through the totem (behaviour from vMaNGOS Aura::HandleAuraDummy case 6495).
-- C++: Custom/Classic/world/talents/classic_talents_mage_shaman.cpp (classic_spell_sha_sentry_totem). Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_sha_sentry_totem';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(6495, 'classic_spell_sha_sentry_totem');
