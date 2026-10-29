-- Classic 1.60: Spearing Strike (warrior talent ability 1310222) deals its extra damage to Giants, Dragonkin and mounted targets and
-- dismounts them (classic_talents_warrior_rogue_hunter.cpp). Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` = 1310222 AND `ScriptName` = 'classic_spell_warr_spearing_strike';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(1310222, 'classic_spell_warr_spearing_strike');
