-- Classic 1.60: Preparation (14185). The client spell is a dummy effect, nothing reset the cooldowns.
-- C++: Custom/Classic/world/talents/classic_rogue_abilities.cpp (classic_spell_rog_preparation). Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` = 14185 AND `ScriptName` = 'classic_spell_rog_preparation';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES (14185, 'classic_spell_rog_preparation');
