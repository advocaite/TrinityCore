-- 1286814 Hoof-Shaped Foot Pedal (item 269902): centaur model, speed only in Desolace (zone 405), combat ends it only outside
-- Desolace. Script: classic_spell_hoof_shaped_foot_pedal. Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` = 1286814 AND `ScriptName` = 'classic_spell_hoof_shaped_foot_pedal';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(1286814, 'classic_spell_hoof_shaped_foot_pedal');
