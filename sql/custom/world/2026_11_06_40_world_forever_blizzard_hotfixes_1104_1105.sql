-- Blizzard's live WoW Forever hotfixes of 2026-11-04 and 2026-11-05 (official patch notes) that our data did not have yet.
--  * Protection warrior, 2026-11-04: Defensive Stance increases all threat by 80% (was 30%). The live value is the hotfixed effect 3
--    of Defensive Stance Passive 7376 (SpellEffect 686475, Threat, base 30 -> 80; SimulationCraft forever data dump of 1.60.1.70790
--    with the 2026-11-05 hotfixes). The client files of 70790 still read 30, so the server takes the live value from classic_spell_value
--    (base points 79 + 1 die side = 80, the same convention as the vanilla rows).
--  * "Escorting Erland" (Silverpine Forest): Deathstalker Erland 1978 respawns much more quickly. Blizzard gives no number; our spawn
--    read the vanilla 300 s, it now uses 60 s, the respawn of the other quest NPCs of the zone (the realm's respawn rate applies on top).
-- Already right in our data, nothing to change here:
--  * Yarrog Baneshadow 3183 (Durotar) respawns in 60 s (spawn 20012303), not the slow live timer the hotfix fixed.
--  * Lotheluum Starbreeze 252359 (Zephras Isle) trains druids since 2026_10_27_00_world_skyborne_class_trainers.sql.
-- Plainsrunning (no stacks in a battleground's preparation, a flag carrier loses them) is in classic_go_scripts.cpp.
-- Safe to run again.

DELETE FROM `classic_spell_value` WHERE `spell_id` = 7376 AND `effect_index` = 2;
INSERT INTO `classic_spell_value` (`spell_id`,`effect_index`,`effect`,`apply_aura`,`base_points`,`die_sides`,`points_per_level`,`misc_value`,`points_per_combo`,`bonus_coeff`,`spell_name`) VALUES
(7376,2,6,10,79,1,0,127,0,-1,'Defensive Stance Passive');

UPDATE `creature` SET `spawntimesecs` = 60 WHERE `id` = 1978;
