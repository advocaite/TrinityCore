-- Soul Link (19028) used to make the demon put 18814 (OLDSoul Link Effect) on the warlock: a hidden, permanent split
-- damage aura, saved with the warlock. The demon now casts 25228 instead. Pet GUIDs are counted from 1 again after a
-- restart, so a saved 18814 could find a new demon as its caster and split damage on top of 25228.
-- Re-runnable.
DELETE FROM `character_aura_effect` WHERE `spell` = 18814;
DELETE FROM `character_aura` WHERE `spell` = 18814;
