-- Classic 1.60: Ferocious Bite (22568, 22827, 22828, 22829, 31018). The client spell has a dummy effect 1 with the energy factor that nothing
-- read, so the energy left after the cost was never converted into damage. C++: Custom/Classic/world/talents/classic_talents_warlock_druid.cpp
-- (classic_spell_dru_ferocious_bite). The retail script bound to the first rank (needs a retail spell) is unbound. Safe to run again.
DELETE FROM `spell_script_names` WHERE (`spell_id` = 22568 AND `ScriptName` = 'spell_dru_ferocious_bite')
    OR (`spell_id` IN (22568, 22827, 22828, 22829, 31018) AND `ScriptName` = 'classic_spell_dru_ferocious_bite');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(22568, 'classic_spell_dru_ferocious_bite'),
(22827, 'classic_spell_dru_ferocious_bite'),
(22828, 'classic_spell_dru_ferocious_bite'),
(22829, 'classic_spell_dru_ferocious_bite'),
(31018, 'classic_spell_dru_ferocious_bite');
