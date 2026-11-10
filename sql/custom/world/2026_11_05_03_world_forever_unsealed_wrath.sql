-- Unsealed Wrath (1310735, Forever paladin talent): "Your melee attacks store a copy of your active Seal for 15 sec, stacking up to 3 times.
-- Landing an attack with a different Seal consumes all stacks, giving the stored Seal its normal chance to activate once per stack."
-- Nothing handled it, so the talent did nothing. The script stores the seal as its Echo aura and replays it on a seal swap.
-- Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` = 1310735 AND `ScriptName` = 'classic_spell_pal_unsealed_wrath';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(1310735, 'classic_spell_pal_unsealed_wrath');
