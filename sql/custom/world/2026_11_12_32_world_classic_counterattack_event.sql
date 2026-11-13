-- Counterattack! (4021, the Barrens) could not be finished: Warlord Krom'zar (9456), whose death drops Krom'zar's Banner (13965 ->
-- gameobject 164690, loot 11000: Piece of Krom'zar's Banner 11227, the quest item), never appeared. In VMaNGOS the quest runs a
-- scripted map event (quest_start_scripts 4021 / gossip_scripts 1882 -> generic_scripts 402110): Kolkar attack the camp, every
-- Kolkar that dies is counted, Lanti'gah yells at 10, Krom'zar and two guards come at 20, and when he dies (or after 20 minutes)
-- the leftover attackers despawn and Regthar Deathgate calls the retreat. The script conversion kept only the summons, and with
-- a 60 s despawn even in combat, so the count, Krom'zar and the end were all missing. No other spawn of 9456 exists (checked on
-- the test realm 2026-11-13: no 9456, 9457, 9458, 9523 or 9524 in `creature`), so this event is the only way to finish the quest.
-- Built from the VMaNGOS world database (world_full_14_june_2021, unchanged by later migrations) in SmartAI:
--   * Regthar Deathgate (3389) runs the event. The summons keep VMaNGOS's manual despawn (type 8) and are cleared at the end.
--   * Kills are counted on Regthar with a counter (his summoned Kolkar Invaders and Stormseers dying).
--   * Only one event at a time (VMaNGOS condition 483): it starts only while no Kolkar is alive within 200 yards of Regthar.
--   * Kolkar Stormseers get faction 655 like VMaNGOS (generic_scripts 402107; Invaders and Krom'zar already have it).
--   * Lanti'gah walks to the west bunker for the fight and goes home after, gossip off and on as in VMaNGOS.
--   * Regthar's "Beware, $n! Look to the west!" now plays: the start list called the event list first, which replaced it.
-- Left out (cosmetic): the two guards joining Krom'zar's formation (402103/402104), the defenders walking to the bunkers
-- (945704/945804) and the Fling Torch the Kolkar cast out of combat (952302/952403).
-- Safe to run again.

DELETE FROM `smart_scripts` WHERE `entryorguid` IN (950000216,950000301) AND `source_type` = 9;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`Difficulties`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`event_param_string`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`action_param7`,`action_param_string`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_param4`,`target_param_string`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(950000216,9,0,0,'',0,0,100,0,0,0,0,0,0,'',1,0,0,0,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'Counterattack quest accepted - Regthar Deathgate - Say Line 0 (VMaNGOS quest_start_scripts 4021)'),
(950000216,9,1,0,'',0,0,100,0,0,0,0,0,0,'',80,950000215,2,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Counterattack quest accepted - Start the event (VMaNGOS quest_start_scripts 4021)'),
(950000301,9,0,0,'',0,0,100,0,0,0,0,0,0,'',1,1,0,0,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'Counterattack gossip - Regthar Deathgate - Say Line 1 (VMaNGOS gossip_scripts 1882)'),
(950000301,9,1,0,'',0,0,100,0,0,0,0,0,0,'',80,950000215,2,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Counterattack gossip - Start the event (VMaNGOS gossip_scripts 1882)');


DELETE FROM `smart_scripts` WHERE `entryorguid` IN (950000215,950000302,950000303) AND `source_type` = 9;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`Difficulties`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`event_param_string`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`action_param7`,`action_param_string`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_param4`,`target_param_string`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(950000215,9,0,0,'',0,0,100,0,0,0,0,0,0,'',63,1,0,1,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Counterattack - Reset Kolkar kill counter'),
(950000215,9,1,0,'',0,0,100,0,0,0,0,0,0,'',67,2,1200000,1200000,0,0,100,0,'',1,0,0,0,0,'',0,0,0,0,'Counterattack - Event time limit 20 min (VMaNGOS map event 4021, 1200 s)'),
(950000215,9,2,0,'',0,0,100,0,2000,2000,0,0,0,'',83,1,0,0,0,0,0,0,'',10,20013100,9990,0,0,'',0,0,0,0,'Counterattack - Lanti''gah - Remove Gossip Flag'),
(950000215,9,3,0,'',0,0,100,0,0,0,0,0,0,'',4,6198,0,0,0,0,0,0,'',10,20013100,9990,0,0,'',0,0,0,0,'Counterattack - Lanti''gah - Play Sound'),
(950000215,9,4,0,'',0,0,100,0,1000,1000,0,0,0,'',45,1,1,0,0,0,0,0,'',10,20013100,9990,0,0,'',0,0,0,0,'Counterattack - Lanti''gah - Set Data 1 1 (move to the west bunker)'),
(950000215,9,5,0,'',0,0,100,0,0,0,0,0,0,'',12,9457,8,60000,0,0,0,0,'',8,0,0,0,0,'',-305.773,-1917.49,91.75,1.74533,'Counterattack - Summon Horde Defender (VMaNGOS generic_scripts 402110, manual despawn)'),
(950000215,9,6,0,'',0,0,100,0,0,0,0,0,0,'',12,9458,8,60000,0,0,0,0,'',8,0,0,0,0,'',-246.604,-1951.93,94.6404,4.76475,'Counterattack - Summon Darkspear Hunter (VMaNGOS generic_scripts 402110, manual despawn)'),
(950000215,9,7,0,'',0,0,100,0,0,0,0,0,0,'',12,9457,8,60000,0,0,0,0,'',8,0,0,0,0,'',-245.64,-1949.9,94.4436,0,'Counterattack - Summon Horde Defender (VMaNGOS generic_scripts 402110, manual despawn)'),
(950000215,9,8,0,'',0,0,100,0,0,0,0,0,0,'',12,9457,8,60000,0,0,0,0,'',8,0,0,0,0,'',-297.332,-1913.34,91.75,2.53073,'Counterattack - Summon Horde Defender (VMaNGOS generic_scripts 402110, manual despawn)'),
(950000215,9,9,0,'',0,0,100,0,0,0,0,0,0,'',12,9458,8,60000,0,0,0,0,'',8,0,0,0,0,'',-275.43,-1904.02,91.75,5.3058,'Counterattack - Summon Darkspear Hunter (VMaNGOS generic_scripts 402110, manual despawn)'),
(950000215,9,10,0,'',0,0,100,0,0,0,0,0,0,'',12,9457,8,60000,0,0,0,0,'',8,0,0,0,0,'',-273.741,-1902.65,91.75,0,'Counterattack - Summon Horde Defender (VMaNGOS generic_scripts 402110, manual despawn)'),
(950000215,9,11,0,'',0,0,100,0,0,0,0,0,0,'',12,9458,8,60000,0,0,0,0,'',8,0,0,0,0,'',-287.059,-1908.5,91.75,1.06465,'Counterattack - Summon Darkspear Hunter (VMaNGOS generic_scripts 402110, manual despawn)'),
(950000215,9,12,0,'',0,0,100,0,0,0,0,0,0,'',12,9457,8,60000,0,0,0,0,'',8,0,0,0,0,'',-285.394,-1906.73,91.75,0,'Counterattack - Summon Horde Defender (VMaNGOS generic_scripts 402110, manual despawn)'),
(950000215,9,13,0,'',0,0,100,0,0,0,0,0,0,'',12,9457,8,60000,0,0,0,0,'',8,0,0,0,0,'',-246.674,-1927.77,92.447,5.14872,'Counterattack - Summon Horde Defender (VMaNGOS generic_scripts 402110, manual despawn)'),
(950000215,9,14,0,'',0,0,100,0,0,0,0,0,0,'',12,9457,8,60000,0,0,0,0,'',8,0,0,0,0,'',-245.753,-1945.85,93.3535,5.32325,'Counterattack - Summon Horde Defender (VMaNGOS generic_scripts 402110, manual despawn)'),
(950000215,9,15,0,'',0,0,100,0,0,0,0,0,0,'',12,9457,8,60000,0,0,0,0,'',8,0,0,0,0,'',-245.677,-1935.89,92.728,5.41052,'Counterattack - Summon Horde Defender (VMaNGOS generic_scripts 402110, manual despawn)'),
(950000215,9,16,0,'',0,0,100,0,0,0,0,0,0,'',12,9457,8,60000,0,0,0,0,'',8,0,0,0,0,'',-245.365,-1925.84,92.4529,0,'Counterattack - Summon Horde Defender (VMaNGOS generic_scripts 402110, manual despawn)'),
(950000215,9,17,0,'',0,0,100,0,0,0,0,0,0,'',12,9523,8,60000,0,0,0,0,'',8,0,0,0,0,'',-197.135,-1895.67,92.697,2.53073,'Counterattack - Summon Kolkar Stormseer (VMaNGOS generic_scripts 402110, manual despawn)'),
(950000215,9,18,0,'',0,0,100,0,0,0,0,0,0,'',12,9524,8,60000,0,0,0,0,'',8,0,0,0,0,'',-270.959,-1829.62,92.6426,3.68265,'Counterattack - Summon Kolkar Invader (VMaNGOS generic_scripts 402110, manual despawn)'),
(950000215,9,19,0,'',0,0,100,0,0,0,0,0,0,'',12,9524,8,60000,0,0,0,0,'',8,0,0,0,0,'',-208.532,-1915.16,93.0361,3.12414,'Counterattack - Summon Kolkar Invader (VMaNGOS generic_scripts 402110, manual despawn)'),
(950000215,9,20,0,'',0,0,100,0,0,0,0,0,0,'',12,9523,8,60000,0,0,0,0,'',8,0,0,0,0,'',-203.927,-1934.67,94.2856,0.767945,'Counterattack - Summon Kolkar Stormseer (VMaNGOS generic_scripts 402110, manual despawn)'),
(950000215,9,21,0,'',0,0,100,0,0,0,0,0,0,'',12,9523,8,60000,0,0,0,0,'',8,0,0,0,0,'',-274.736,-1850.43,93.2679,0.872665,'Counterattack - Summon Kolkar Stormseer (VMaNGOS generic_scripts 402110, manual despawn)'),
(950000215,9,22,0,'',0,0,100,0,0,0,0,0,0,'',12,9524,8,60000,0,0,0,0,'',8,0,0,0,0,'',-203.835,-1956.34,94.2504,3.26377,'Counterattack - Summon Kolkar Invader (VMaNGOS generic_scripts 402110, manual despawn)'),
(950000215,9,23,0,'',0,0,100,0,0,0,0,0,0,'',12,9524,8,60000,0,0,0,0,'',8,0,0,0,0,'',-189.203,-1937.01,94.1333,2.04204,'Counterattack - Summon Kolkar Invader (VMaNGOS generic_scripts 402110, manual despawn)'),
(950000215,9,24,0,'',0,0,100,0,0,0,0,0,0,'',12,9524,8,60000,0,0,0,0,'',8,0,0,0,0,'',-229.671,-1922.15,92.8292,1.18682,'Counterattack - Summon Kolkar Invader (VMaNGOS generic_scripts 402110, manual despawn)'),
(950000215,9,25,0,'',0,0,100,0,0,0,0,0,0,'',12,9524,8,60000,0,0,0,0,'',8,0,0,0,0,'',-288.108,-1836.18,92.8948,2.96706,'Counterattack - Summon Kolkar Invader (VMaNGOS generic_scripts 402110, manual despawn)'),
(950000215,9,26,0,'',0,0,100,0,0,0,0,0,0,'',12,9524,8,60000,0,0,0,0,'',8,0,0,0,0,'',-326.682,-1867.33,93.9783,2.49582,'Counterattack - Summon Kolkar Invader (VMaNGOS generic_scripts 402110, manual despawn)'),
(950000215,9,27,0,'',0,0,100,0,0,0,0,0,0,'',12,9524,8,60000,0,0,0,0,'',8,0,0,0,0,'',-219.713,-1932.09,93.7473,2.35619,'Counterattack - Summon Kolkar Invader (VMaNGOS generic_scripts 402110, manual despawn)'),
(950000215,9,28,0,'',0,0,100,0,0,0,0,0,0,'',12,9523,8,60000,0,0,0,0,'',8,0,0,0,0,'',-183.963,-1870.13,92.5011,4.29351,'Counterattack - Summon Kolkar Stormseer (VMaNGOS generic_scripts 402110, manual despawn)'),
(950000215,9,29,0,'',0,0,100,0,0,0,0,0,0,'',12,9523,8,60000,0,0,0,0,'',8,0,0,0,0,'',-285.988,-1867.18,92.7527,4.08407,'Counterattack - Summon Kolkar Stormseer (VMaNGOS generic_scripts 402110, manual despawn)'),
(950000215,9,30,0,'',0,0,100,0,0,0,0,0,0,'',12,9524,8,60000,0,0,0,0,'',8,0,0,0,0,'',-182.836,-1918,94.1594,0.890118,'Counterattack - Summon Kolkar Invader (VMaNGOS generic_scripts 402110, manual despawn)'),
(950000215,9,31,0,'',0,0,100,0,0,0,0,0,0,'',12,9523,8,60000,0,0,0,0,'',8,0,0,0,0,'',-170.449,-1901.03,94.3928,5.35816,'Counterattack - Summon Kolkar Stormseer (VMaNGOS generic_scripts 402110, manual despawn)'),
(950000215,9,32,0,'',0,0,100,0,0,0,0,0,0,'',12,9524,8,60000,0,0,0,0,'',8,0,0,0,0,'',-340.44,-1853.17,93.8764,3.19395,'Counterattack - Summon Kolkar Invader (VMaNGOS generic_scripts 402110, manual despawn)'),
(950000215,9,33,0,'',0,0,100,0,0,0,0,0,0,'',12,9524,8,60000,0,0,0,0,'',8,0,0,0,0,'',-305.295,-1870.51,92.7883,6.19592,'Counterattack - Summon Kolkar Invader (VMaNGOS generic_scripts 402110, manual despawn)'),
(950000215,9,34,0,'',0,0,100,0,0,0,0,0,0,'',12,9523,8,60000,0,0,0,0,'',8,0,0,0,0,'',-306.303,-1840.31,95.1089,0.383972,'Counterattack - Summon Kolkar Stormseer (VMaNGOS generic_scripts 402110, manual despawn)'),
(950000215,9,35,0,'',0,0,100,0,0,0,0,0,0,'',12,9524,8,60000,0,0,0,0,'',8,0,0,0,0,'',-290.724,-1853.01,92.6655,5.49779,'Counterattack - Summon Kolkar Invader (VMaNGOS generic_scripts 402110, manual despawn)'),
(950000215,9,36,0,'',0,0,100,0,0,0,0,0,0,'',12,9524,8,60000,0,0,0,0,'',8,0,0,0,0,'',-312.151,-1858.05,94.254,3.50811,'Counterattack - Summon Kolkar Invader (VMaNGOS generic_scripts 402110, manual despawn)'),
(950000215,9,37,0,'',0,0,100,0,0,0,0,0,0,'',12,9524,8,60000,0,0,0,0,'',8,0,0,0,0,'',-288.412,-1886.38,92.6818,5.20108,'Counterattack - Summon Kolkar Invader (VMaNGOS generic_scripts 402110, manual despawn)'),
(950000215,9,38,0,'',0,0,100,0,0,0,0,0,0,'',12,9524,8,60000,0,0,0,0,'',8,0,0,0,0,'',-329.012,-1847.05,95.2063,5.18363,'Counterattack - Summon Kolkar Invader (VMaNGOS generic_scripts 402110, manual despawn)'),
(950000215,9,39,0,'',0,0,100,0,0,0,0,0,0,'',12,9524,8,60000,0,0,0,0,'',8,0,0,0,0,'',-341.506,-1870.58,93.4688,4.71239,'Counterattack - Summon Kolkar Invader (VMaNGOS generic_scripts 402110, manual despawn)'),
(950000302,9,0,0,'',0,0,100,0,0,0,0,0,0,'',12,9456,1,600000,0,0,0,0,'',8,0,0,0,0,'',-297.624,-1839.22,93.8783,4.81711,'Counterattack - Summon Warlord Krom''zar (VMaNGOS generic_scripts 402102)'),
(950000302,9,1,0,'',0,0,100,0,1000,1000,0,0,0,'',12,9524,1,600000,0,0,0,0,'',8,0,0,0,0,'',-293.931,-1837.24,93.4963,4.43314,'Counterattack - Summon Kolkar Invader with Krom''zar'),
(950000302,9,2,0,'',0,0,100,0,0,0,0,0,0,'',12,9523,1,600000,0,0,0,0,'',8,0,0,0,0,'',-297.878,-1834.64,94.2335,4.79965,'Counterattack - Summon Kolkar Stormseer with Krom''zar'),
(950000303,9,0,0,'',0,0,100,0,0,0,0,0,0,'',74,2,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Counterattack over - Remove the time limit'),
(950000303,9,1,0,'',0,0,100,0,0,0,0,0,0,'',63,1,0,1,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Counterattack over - Reset Kolkar kill counter'),
(950000303,9,2,0,'',0,0,100,0,0,0,0,0,0,'',1,2,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Counterattack over - Regthar Deathgate - Say Line 2 (VMaNGOS generic_scripts 402106)'),
(950000303,9,3,0,'',0,0,100,0,0,0,0,0,0,'',41,3000,0,0,0,0,0,0,'',11,9524,200,0,0,'',0,0,0,0,'Counterattack over - Despawn the event''s Kolkar Invaders (VMaNGOS generic_scripts 402105)'),
(950000303,9,4,0,'',0,0,100,0,0,0,0,0,0,'',41,3000,0,0,0,0,0,0,'',11,9523,200,0,0,'',0,0,0,0,'Counterattack over - Despawn the event''s Kolkar Stormseers (VMaNGOS generic_scripts 402105)'),
(950000303,9,5,0,'',0,0,100,0,0,0,0,0,0,'',41,3000,0,0,0,0,0,0,'',11,9457,200,0,0,'',0,0,0,0,'Counterattack over - Despawn the event''s Horde Defenders (VMaNGOS generic_scripts 402105)'),
(950000303,9,6,0,'',0,0,100,0,0,0,0,0,0,'',41,3000,0,0,0,0,0,0,'',11,9458,200,0,0,'',0,0,0,0,'Counterattack over - Despawn the event''s Darkspear Hunters (VMaNGOS generic_scripts 402105)'),
(950000303,9,7,0,'',0,0,100,0,1000,1000,0,0,0,'',4,6199,0,0,0,0,0,0,'',10,20013100,9990,0,0,'',0,0,0,0,'Counterattack over - Lanti''gah - Play Sound'),
(950000303,9,8,0,'',0,0,100,0,0,0,0,0,0,'',82,1,0,0,0,0,0,0,'',10,20013100,9990,0,0,'',0,0,0,0,'Counterattack over - Lanti''gah - Add Gossip Flag'),
(950000303,9,9,0,'',0,0,100,0,1000,1000,0,0,0,'',45,1,2,0,0,0,0,0,'',10,20013100,9990,0,0,'',0,0,0,0,'Counterattack over - Lanti''gah - Set Data 1 2 (go home)');

DELETE FROM `smart_scripts` WHERE `entryorguid` = 3389 AND `source_type` = 0 AND `id` BETWEEN 10 AND 15;
DELETE FROM `smart_scripts` WHERE `entryorguid` = 9990 AND `source_type` = 0 AND `id` BETWEEN 1 AND 3;
DELETE FROM `smart_scripts` WHERE `entryorguid` = 9523 AND `source_type` = 0 AND `id` = 3;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`Difficulties`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`event_param_string`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`action_param7`,`action_param_string`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_param4`,`target_param_string`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(3389,0,10,0,'',82,0,100,0,9524,0,0,0,0,'',63,1,1,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Regthar Deathgate - Summoned Kolkar Invader dies - Count Kolkar kill (VMaNGOS map event 4021 data)'),
(3389,0,11,0,'',82,0,100,0,9523,0,0,0,0,'',63,1,1,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Regthar Deathgate - Summoned Kolkar Stormseer dies - Count Kolkar kill (VMaNGOS map event 4021 data)'),
(3389,0,12,0,'',77,0,100,0,1,10,0,0,0,'',1,0,0,0,0,0,0,0,'',10,20013100,9990,0,0,'',0,0,0,0,'Regthar Deathgate - 10 Kolkar killed - Lanti''gah yells (VMaNGOS generic_scripts 402101)'),
(3389,0,13,0,'',77,0,100,0,1,20,0,0,0,'',80,950000302,2,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Regthar Deathgate - 20 Kolkar killed - Warlord Krom''zar comes (VMaNGOS generic_scripts 402102)'),
(3389,0,14,0,'',82,0,100,0,9456,0,0,0,0,'',80,950000303,2,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Regthar Deathgate - Warlord Krom''zar dies - Counterattack over (VMaNGOS 945601, 402105, 402106)'),
(3389,0,15,0,'',59,0,100,0,2,0,0,0,0,'',80,950000303,2,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Regthar Deathgate - 20 min passed - Counterattack over (VMaNGOS map event 4021 time limit)'),
(9990,0,1,0,'',38,0,100,0,1,1,0,0,0,'',69,1,0,0,0,0,0,0,'',8,0,0,0,0,'',-318.461,-1897.02,91.8194,0,'Lanti''gah - On Data Set 1 1 - Move to the west bunker (VMaNGOS generic_scripts 402110)'),
(9990,0,2,0,'',38,0,100,0,1,2,0,0,0,'',69,2,0,0,0,0,0,0,'',8,0,0,0,0,'',-294.827,-1913.27,91.7916,0,'Lanti''gah - On Data Set 1 2 - Move home (VMaNGOS generic_scripts 402106)'),
(9990,0,3,0,'',34,0,100,0,8,2,0,0,0,'',89,5,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Lanti''gah - Reached home - Move Random 5 (VMaNGOS generic_scripts 402106)'),
(9523,0,3,0,'',54,0,100,0,0,0,0,0,0,'',2,655,0,0,0,0,0,0,'',1,0,0,0,0,'',0,0,0,0,'Kolkar Stormseer - Just Summoned - Set Faction 655 (VMaNGOS generic_scripts 402107)');

-- Only one Counterattack at a time: Regthar starts it (quest accept, smart event 1; gossip, smart event 3) only while no
-- Kolkar Invader or Stormseer is alive within 200 yards (VMaNGOS condition 483, event 4021 not running).
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 22 AND `SourceEntry` = 3389 AND `SourceId` = 0 AND `SourceGroup` IN (2,4);
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
(22,2,3389,0,0,29,1,9524,200,0,'',1,0,0,'','Regthar Deathgate - start Counterattack on quest accept only if no Kolkar Invader is alive nearby'),
(22,2,3389,0,0,29,1,9523,200,0,'',1,0,0,'','Regthar Deathgate - start Counterattack on quest accept only if no Kolkar Stormseer is alive nearby'),
(22,4,3389,0,0,29,1,9524,200,0,'',1,0,0,'','Regthar Deathgate - start Counterattack from gossip only if no Kolkar Invader is alive nearby'),
(22,4,3389,0,0,29,1,9523,200,0,'',1,0,0,'','Regthar Deathgate - start Counterattack from gossip only if no Kolkar Stormseer is alive nearby');

DELETE FROM `creature_text` WHERE (`CreatureID` = 9990 AND `GroupID` = 0) OR (`CreatureID` = 3389 AND `GroupID` = 2);
INSERT INTO `creature_text` (`CreatureID`,`GroupID`,`ID`,`Text`,`Type`,`Language`,`Probability`,`Emote`,`Duration`,`Sound`,`SoundPlayType`,`BroadcastTextId`,`TextRange`,`comment`) VALUES
(9990,0,0,'Half way there...',14,0,100,0,0,0,0,8439,0,'Lanti''gah - Counterattack 10 kills (VMaNGOS generic_scripts 402101)'),
(3389,2,0,'The Kolkar invaders are retreating!',14,0,100,0,0,0,0,8897,0,'Regthar Deathgate - Counterattack over (VMaNGOS generic_scripts 402106)');
