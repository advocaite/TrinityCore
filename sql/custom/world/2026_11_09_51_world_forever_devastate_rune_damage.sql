-- Devastate 403196, the damage part of the Rune of Devastate: only Sunder Armor may cast it (through the rune aura 403195).
-- A player who pressed it directly hit anything in sight, with no rage, no cooldown and no range limit.
DELETE FROM `spell_script_names` WHERE `spell_id`=403196 AND `ScriptName`='spell_warr_devastate_rune_damage';
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
(403196,'spell_warr_devastate_rune_damage');
