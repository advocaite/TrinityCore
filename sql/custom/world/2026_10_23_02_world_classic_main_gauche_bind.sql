-- Main Gauche (1289849): bound as the client data has it. SpellAuraOptions ProcChance 100 at every rank; the trait curve 83032 scales the
-- off-hand strike's damage percentage 17 / 34 / 50 % at rank 1 / 2 / 3.
DELETE FROM `spell_script_names` WHERE `spell_id` = 1289849;
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES (1289849,'classic_spell_rog_main_gauche');
