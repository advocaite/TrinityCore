-- Classic 1.60: Vanish (behaviour from vMaNGOS RogueVanishScript). The retail vanish scripts bound to it need retail spells and never ran.
-- C++: Custom/Classic/world/talents/classic_rogue_abilities.cpp (classic_spell_rog_vanish). Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_rog_vanish';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES (1856,'classic_spell_rog_vanish'),(1857,'classic_spell_rog_vanish'),(27617,'classic_spell_rog_vanish');
