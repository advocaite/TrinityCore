-- Anubisath Shadow Storm (14297, 26546, 26555) only hits players 25 yards or more from the caster, as in vanilla.
-- C++: classic_spell_shadow_storm_min_range (Custom/Classic/world/classic_spell_scripts.cpp). Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_shadow_storm_min_range';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(14297, 'classic_spell_shadow_storm_min_range'),
(26546, 'classic_spell_shadow_storm_min_range'),
(26555, 'classic_spell_shadow_storm_min_range');
