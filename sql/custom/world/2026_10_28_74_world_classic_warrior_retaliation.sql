-- Classic 1.60: Retaliation (20230) counterattacks melee attacks from the front (classic_talents_warrior_rogue_hunter.cpp). The
-- counterattack 22858 has the Self Only range: SpellMgr.cpp gives it the melee range. Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` = 20230 AND `ScriptName` = 'classic_spell_warr_retaliation';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(20230, 'classic_spell_warr_retaliation');
