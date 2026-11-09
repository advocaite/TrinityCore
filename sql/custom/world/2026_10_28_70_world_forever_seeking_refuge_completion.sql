-- Seeking Refuge 99144 (Discord, Beastmade 2026-10-28): Bareth Dawnstone walks his whole route but the quest never completes.
-- Our quest template (sniff baseline) carries QUEST_FLAGS_COMPLETION_EVENT (0x2), so besides the escort objective the quest
-- also needs its completion event before it can complete. The other Forever server escort from 2026_10_26_38 only gives the objective
-- credit (the other three escorts there have no such flag, so credit alone completes them), and the route is started on
-- Bareth himself, so the escort end never fires the event either. At the end of the route the players within 40 yd now get
-- the objective credit and then the completion event. Safe to run again.
DELETE FROM `smart_scripts` WHERE `entryorguid`=275954 AND `source_type`=0 AND `id` BETWEEN 12 AND 14;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_chance`,`event_param1`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`target_type`,`target_param2`,`comment`) VALUES
(275954,0,12,13,58,100,0,33,275954,0,0,0,17,40,'escort 99144 - Route ended - Escort credit to players within 40 yd (INFERRED)'),
(275954,0,13,14,61,100,0,152,99144,0,0,0,17,40,'escort 99144 - Linked - Completion event of quest 99144 for players within 40 yd'),
(275954,0,14,0,61,100,0,41,5000,0,0,0,1,0,'escort 99144 - Linked - Despawn, respawn at the start');
