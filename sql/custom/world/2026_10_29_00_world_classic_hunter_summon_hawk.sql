-- Classic 1.60: Summon Hawk (hunter talent 1293241 and ranks 1293525, 1293526, 1293527) adds its ranged attack power damage, and its
-- missile (1312639) summons the hawk that keeps attacking (classic_talents_warrior_rogue_hunter.cpp). Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` IN (1293241, 1293525, 1293526, 1293527, 1312639) AND `ScriptName` IN ('classic_spell_hun_summon_hawk', 'classic_spell_hun_summon_hawk_missile');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(1293241, 'classic_spell_hun_summon_hawk'),
(1293525, 'classic_spell_hun_summon_hawk'),
(1293526, 'classic_spell_hun_summon_hawk'),
(1293527, 'classic_spell_hun_summon_hawk'),
(1312639, 'classic_spell_hun_summon_hawk_missile');
