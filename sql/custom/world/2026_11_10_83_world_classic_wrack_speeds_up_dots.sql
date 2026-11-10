-- Classic 1.60.1.70888 Wrack (1316697): while channelled, the warlock's Corruption, Siphon Life and Bane of Agony on the target tick
-- faster (effect 2, a dummy aura; the record is in 2026_11_10_80_hotfixes_forever_live_pushes_70888). Script classic_spell_warl_wrack
-- in src/server/scripts/Custom/Classic/world/talents/classic_talents_warlock_druid.cpp. Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` = 1316697 AND `ScriptName` = 'classic_spell_warl_wrack';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(1316697, 'classic_spell_warl_wrack');
