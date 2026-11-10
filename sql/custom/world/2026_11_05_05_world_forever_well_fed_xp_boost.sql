-- Forever Well Fed buffs: Well Fed XP Boost (1243969, +5% experience from kills) comes with the food while the client's condition for
-- the boost holds (PlayerCondition 142418). Nothing applied it before. Safe to run again.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_well_fed_xp_boost';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(1248406, 'classic_spell_well_fed_xp_boost'),
(1248420, 'classic_spell_well_fed_xp_boost'),
(1248421, 'classic_spell_well_fed_xp_boost'),
(1248422, 'classic_spell_well_fed_xp_boost'),
(1248688, 'classic_spell_well_fed_xp_boost'),
(1249519, 'classic_spell_well_fed_xp_boost'),
(1249520, 'classic_spell_well_fed_xp_boost'),
(1249521, 'classic_spell_well_fed_xp_boost'),
(1249523, 'classic_spell_well_fed_xp_boost'),
(1249907, 'classic_spell_well_fed_xp_boost'),
(1249926, 'classic_spell_well_fed_xp_boost'),
(1249927, 'classic_spell_well_fed_xp_boost'),
(1319310, 'classic_spell_well_fed_xp_boost');
