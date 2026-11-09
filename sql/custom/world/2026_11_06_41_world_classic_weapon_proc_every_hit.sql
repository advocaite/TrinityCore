-- Classic 1.60: Sword of Corruption 13032 procced on every hit. Its chance-on-hit spell 17510 reads a chance of exactly 100 in the
-- 1.60.1.70790 game files and the item had no rate of its own (2026_10_11_25_world_classic_item_proc_ppm.sql is VMaNGOS's list and
-- does not hold it), so Player::CastItemCombatSpell rolled 100%. It gets 1 proc per minute, the rate wowsims/forever (MIT,
-- https://github.com/wowsims/forever, tools/database/overrides/weapon_procs.go) carries over from vanilla.
-- The other weapons of that list either have a VMaNGOS rate here already or read a chance of 0 / above 100, which our core turns into
-- the weapon speed rate (about 1 per minute): they keep that. Safe to run again.
INSERT INTO `item_template_addon` (`Id`,`SpellPPMChance`) VALUES (13032,1)
ON DUPLICATE KEY UPDATE `SpellPPMChance` = IF(`SpellPPMChance` = 0, VALUES(`SpellPPMChance`), `SpellPPMChance`);
