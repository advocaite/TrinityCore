-- Alterac Mountains, Dalaran Summoner (2358): WoW Forever fixed Summoners that "summon too many" (Blizzard notes of 2 November 2026).
-- Here: each Summoner calls a Dalaran Shield Guard (2271, spell 3655) and a Dalaran Theurgist (2272, spell 3658) once per fight
-- (50% each). The summons have no duration (despawn only on death), and the Summoner's once-per-fight events start again after it
-- resets. A player who runs off leaves the guards standing; every new pull adds two more, and the camps fill up with guards.
-- Fix: a summoned Shield Guard or Theurgist (event phase 1, set when summoned) despawns when it gives up the fight. Placed ones
-- (phase 0) are untouched. Safe to run again.
DELETE FROM `smart_scripts` WHERE `source_type` = 0 AND ((`entryorguid` = 2271 AND `id` IN (1,2)) OR (`entryorguid` = 2272 AND `id` IN (3,4)));
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`Difficulties`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`event_param_string`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`action_param7`,`action_param_string`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_param4`,`target_param_string`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(2271,0,1,0,'',54,0,100,0,0,0,0,0,0,'',22,1,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Dalaran Shield Guard - Just Summoned - Set Event Phase 1'),
(2271,0,2,0,'',7,1,100,0,0,0,0,0,0,'',41,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Dalaran Shield Guard - On Evade (summoned) - Despawn'),
(2272,0,3,0,'',54,0,100,0,0,0,0,0,0,'',22,1,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Dalaran Theurgist - Just Summoned - Set Event Phase 1'),
(2272,0,4,0,'',7,1,100,0,0,0,0,0,0,'',41,0,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Dalaran Theurgist - On Evade (summoned) - Despawn');
