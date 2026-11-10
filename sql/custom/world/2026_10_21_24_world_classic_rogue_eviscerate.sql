-- Classic 1.60: Eviscerate grows with attack power (patch 1.12, behaviour from vMaNGOS RogueEviscerateScript).
-- C++: Custom/Classic/world/talents/classic_rogue_abilities.cpp (classic_spell_rog_eviscerate). Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_rog_eviscerate';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(2098,'classic_spell_rog_eviscerate'),(6760,'classic_spell_rog_eviscerate'),(6761,'classic_spell_rog_eviscerate'),(6762,'classic_spell_rog_eviscerate'),
(8623,'classic_spell_rog_eviscerate'),(8624,'classic_spell_rog_eviscerate'),(11299,'classic_spell_rog_eviscerate'),(11300,'classic_spell_rog_eviscerate'),
(31016,'classic_spell_rog_eviscerate');
