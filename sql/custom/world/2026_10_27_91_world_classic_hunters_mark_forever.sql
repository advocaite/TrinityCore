-- Hunter's Mark rank 4 (14325): client 70630 raised the ranged attack power bonus from 72 to 144 (70532 had rank 4 below rank 3).
-- The vanilla value row (110) is now below Forever's, so it goes and Forever's value applies. Safe to run again.
DELETE FROM `classic_spell_value` WHERE `spell_id`=14325 AND `effect_index`=1;
