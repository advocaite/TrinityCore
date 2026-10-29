-- Classic 1.60: Last Stand (12975) gives the warrior 30% of their maximum health (classic_talents_warrior_rogue_hunter.cpp).
-- Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` = 12975 AND `ScriptName` = 'classic_spell_warr_last_stand';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(12975, 'classic_spell_warr_last_stand');
