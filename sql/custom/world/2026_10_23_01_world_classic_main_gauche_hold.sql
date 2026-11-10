-- Main Gauche (1289849): the spell data gives ProcChance 100 and no procs-per-minute entry, which would be an off-hand strike on every main-hand
-- swing; the description only says "a chance". The script stays in the code but is unbound until the real chance is read from the trait node
-- (TraitDefinitionEffectPoints of the talent) and set in a spell_proc row.
DELETE FROM `spell_script_names` WHERE `spell_id` = 1289849 AND `ScriptName` = 'classic_spell_rog_main_gauche';
