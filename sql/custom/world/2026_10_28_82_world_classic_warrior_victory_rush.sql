-- Classic 1.60: Victory Rush (402927) deals its attack power damage and uses up Victorious (classic_talents_warrior_rogue_hunter.cpp).
-- Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` = 402927 AND `ScriptName` = 'classic_spell_warr_victory_rush';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(402927, 'classic_spell_warr_victory_rush');
