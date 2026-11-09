-- Two client values that fall below the rank under them, which no rank table does; vanilla 1.12 values (VMaNGOS spell_template, newest row of build <= 5875, GPL-2.0):
--  * Hunter's Mark rank 4 (14325), effect 2 (ranged attack power versus the target): client 71, rank 3 is 98; vanilla 110 (effectBasePoints 109)
--  * Trueshot Aura rank 3 (20906), effect 1 (ranged attack power): client 50, rank 2 is 75; vanilla 100 (effectBasePoints 99)
-- The other differences of the 2026-10-22 class audit (class-spell-audit/*.md) are left on the client's Forever values: they are not out of rank order.
DELETE FROM `classic_spell_value` WHERE (`spell_id`, `effect_index`) IN ((14325, 1), (20906, 0));
INSERT INTO `classic_spell_value` (`spell_id`,`effect_index`,`effect`,`apply_aura`,`base_points`,`die_sides`,`points_per_level`,`misc_value`,`points_per_combo`,`bonus_coeff`,`spell_name`) VALUES
(14325,1,6,127,109,1,0,0,0,-1,'Hunter''s Mark'),
(20906,0,35,124,99,1,0,0,0,-1,'Trueshot Aura');
