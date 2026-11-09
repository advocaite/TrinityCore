-- FULL FOREVER RESERVE (world). Removes the vanilla value rows that hold these spells over live Forever, so the client / live
-- hotfix value is used: Aspect of the Hawk 13165/14318-14321, Lesser Intellect 3166, Accelerant 11069 (vanilla -0.1 sec per
-- rank; live -0.5 sec). Run with 01_hotfixes_live_spell_values.sql. Undo: 02_undo.sql. Safe to run again.
DELETE FROM `classic_spell_value` WHERE `spell_id` IN (13165,14318,14319,14320,14321,3166,11069);
