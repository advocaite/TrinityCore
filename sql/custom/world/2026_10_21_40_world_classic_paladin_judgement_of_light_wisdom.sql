-- Classic 1.60: Judgement of Light / Wisdom. The judgement debuff procs a dummy spell from the judged enemy (5373 "Judgement of Light Intermediate",
-- 1826 "Judgement of Wisdom Intermediate"); the script casts the rank's heal / mana spell from the paladin on whoever hit the enemy
-- (classic_spell_scripts.cpp, classic_spell_pal_judgement_intermediate). Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_pal_judgement_intermediate';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(5373, 'classic_spell_pal_judgement_intermediate'),
(1826, 'classic_spell_pal_judgement_intermediate');
