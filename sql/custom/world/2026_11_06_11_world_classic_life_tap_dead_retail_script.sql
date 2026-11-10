-- Classic 1.60 (WoW Forever): Life Tap rank 1 (1454) still carried the retail binding 'spell_gen_trigger_exclude_caster_aura_spell',
-- missed by 2026_10_27_81. That script needs the spell's ExcludeCasterAuraSpell, and Life Tap has no aura restriction in the 1.60 client
-- (70790 SpellAuraRestrictions has no row for any rank), so Validate() fails at every start and the binding is dropped. Nothing changes
-- in game: all six ranks keep 'classic_talent_warl_life_tap' from 2026_10_11_33. Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_gen_trigger_exclude_caster_aura_spell' AND `spell_id` = 1454;
