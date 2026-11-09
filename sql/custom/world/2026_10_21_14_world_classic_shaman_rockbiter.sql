-- Classic 1.60: Rockbiter Weapon proc threat (behaviour from vMaNGOS Spell::EffectScriptEffect, shaman cases 20865-20871).
-- C++: Custom/Classic/world/talents/classic_talents_mage_shaman.cpp (classic_spell_sha_rockbiter_threat). Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_sha_rockbiter_threat';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(20860, 'classic_spell_sha_rockbiter_threat'),
(20865, 'classic_spell_sha_rockbiter_threat'),
(20866, 'classic_spell_sha_rockbiter_threat'),
(20867, 'classic_spell_sha_rockbiter_threat'),
(20868, 'classic_spell_sha_rockbiter_threat'),
(20870, 'classic_spell_sha_rockbiter_threat'),
(20871, 'classic_spell_sha_rockbiter_threat');
