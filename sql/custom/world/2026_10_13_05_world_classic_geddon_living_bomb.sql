-- Classic 1.60: the Living Bomb (20475) of Baron Geddon explodes with Geddon as the original caster (flat vanilla 3200, no spell power / crit of the
-- carrier), script classic_spell_geddon_living_bomb (classic_boss_baron_geddon.cpp). Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` = 20475 OR `ScriptName` = 'classic_spell_geddon_living_bomb';
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
(20475,'classic_spell_geddon_living_bomb');
