-- Classic 1.60 (vanilla): Flash of Light heals with healing power. VMaNGOS heals through the shared spell 19993 (the rank's script effect casts it with the
-- rolled amount), so the rank rows carry effectBonusCoefficient 0 and 19993 carries 0.429 (1.5 s / 3.5). Here the rank heals directly, and the 0 taken across
-- by 2026_10_10_21 made it ignore healing power. Same change as the generator (tools/classic_spell_values/gen_spell_values.py). Safe to run again.
UPDATE `classic_spell_value` SET `bonus_coeff` = 0.429 WHERE `spell_id` IN (19750, 19939, 19940, 19941, 19942, 19943) AND `effect_index` = 0 AND `bonus_coeff` = 0;
