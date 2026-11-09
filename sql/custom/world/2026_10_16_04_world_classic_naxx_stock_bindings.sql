-- Naxxramas: three stock TrinityCore spell scripts still bound on vanilla spell ids over the Classic ports
-- (2026_09_27_00_world_forever_baseline_02.sql spell_script_names), the same class of double binding as Geddon's Inferno.
-- 28089 Polarity Shift: classic_boss_thaddius gives out the charges itself 3 s after the cast; the stock spell_thaddius_polarity_shift
--   handed every player a random charge on hit and raced it, so neither side was a balanced split.
-- 27819 Detonate Mana: the stock spell_kelthuzad_detonate_mana burns a tenth of the carrier's maximum mana and deals ten times that
--   (the whole pool) to everyone within 10 yards. Vanilla burns half of the carrier's current mana and deals that much:
--   classic_spell_kelthuzad_detonate_mana (naxx/classic_boss_kelthuzad.cpp) replaces it.
-- 29865 Poison Aura (Loatheb): the stock spell_loatheb_deathbloom casts the retail Deathbloom burst (55594) from each raider when the
--   12 s aura lapses; vanilla has no such burst.
-- Safe to run again.
DELETE FROM `spell_script_names` WHERE (`spell_id`, `ScriptName`) IN (
(28089, 'spell_thaddius_polarity_shift'),
(27819, 'spell_kelthuzad_detonate_mana'),
(29865, 'spell_loatheb_deathbloom'));
DELETE FROM `spell_script_names` WHERE `spell_id` = 27819 AND `ScriptName` = 'classic_spell_kelthuzad_detonate_mana';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES (27819, 'classic_spell_kelthuzad_detonate_mana');
