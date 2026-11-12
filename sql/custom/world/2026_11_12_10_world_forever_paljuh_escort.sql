-- Lost in the Shadows 99123 (noobiru, 0.1.35): Pal'juh 275811 walks off ahead, vanishes about halfway and the quest cannot be
-- turned in to Master Vornal. Halfway is the end of her route (node 6, south of Kolkar Crag): she gives the escort credit there
-- and despawns. The credit only reached players within 40 yd of her, and the route was started on Pal'juh herself, so the
-- escort never knew who it was escorting: a player held up by a fight behind her lost the credit with no sign of it.
-- Now the route is started for the player who took the quest (the escort fails, with the quest failed message, when that
-- player and the group are more than 60 yd away, as for every SmartAI escort), and the credit at the end reaches every
-- player within those 60 yd. She came back only after 5 minutes (spawn time 300 s); now after 60 s. INFERRED: no sniff of
-- this escort exists, the respawn time is a guess. Safe to run again.
UPDATE `smart_scripts` SET `target_type`=7, `target_param1`=0, `target_param2`=0
WHERE `entryorguid`=275811 AND `source_type`=0 AND `id`=11 AND `action_type`=53;
UPDATE `smart_scripts` SET `target_param2`=60, `comment`='escort 99123 - Route ended - Escort credit to players within 60 yd'
WHERE `entryorguid`=275811 AND `source_type`=0 AND `id`=12 AND `action_type`=33;
UPDATE `creature` SET `spawntimesecs`=60 WHERE `guid`=27581100 AND `id`=275811;
