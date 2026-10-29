-- Classic 1.60: Hemorrhage (16511) and Ghostly Strike (14278) deal their dagger weapon damage with a dagger in the main hand
-- (classic_talents_warrior_rogue_hunter.cpp). Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` IN (16511, 14278) AND `ScriptName` IN ('classic_spell_rog_hemorrhage', 'classic_spell_rog_ghostly_strike');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(16511, 'classic_spell_rog_hemorrhage'),
(14278, 'classic_spell_rog_ghostly_strike');
