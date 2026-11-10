-- 590 A Rogue's Deal (Tirisfal): Calvin Montague turned hostile on accept but stayed immune to players, so he could not be
-- fought, and the quest never completed. VMaNGOS: on accept he loses Immune to PC, turns hostile (faction 168), cannot drop below
-- 5% and attacks; at 20% he turns passive and evades; back home he begs a minute's rest, eats, and the map event ends as a
-- success that completes the quest for players within 40 yards. If the player dies or runs (40 yards) first, the event fails:
-- faction and flags are restored and the quest fails.
-- Phases: 1 idle (set on reset), 2 fight (set on accept), 3 beaten. Only a beaten Calvin who reaches home completes the quest;
-- one who evades from the fight fails it.

-- On accept: also drop Immune to PC (he attacks 1 s in) and remember the player.
DELETE FROM `smart_scripts` WHERE `entryorguid`=950000026 AND `source_type`=9 AND `id` IN (5,6);
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(950000026,9,5,0,0,0,100,0,0,0,0,0,144,0,0,0,0,0,0,1,0,0,0,0,0,0,0,'Calvin Montague - Set Immune To PC Off (VMaNGOS quest_start_scripts 590)'),
(950000026,9,6,0,0,0,100,0,0,0,0,0,64,1,0,0,0,0,0,7,0,0,0,0,0,0,0,'Calvin Montague - Store Quest Player');

-- The converted "back to phase 1 on reset" ran on every reset, and SmartAI resets just before Reached Home, so the phased home
-- events never fired. It now only applies when he is idle anyway (phase 1); a spawn starts in phase 0, which is idle too.
UPDATE `smart_scripts` SET `event_phase_mask`=1 WHERE `entryorguid`=6784 AND `source_type`=0 AND `id`=0 AND `event_type`=25;

-- Beaten at 20% (fight phase only): evade, immune to players again, phase 3.
UPDATE `smart_scripts` SET `event_phase_mask`=2, `link`=4 WHERE `entryorguid`=6784 AND `source_type`=0 AND `id`=1;
UPDATE `smart_scripts` SET `event_phase_mask`=4 WHERE `entryorguid`=6784 AND `source_type`=0 AND `id`=2;
DELETE FROM `smart_scripts` WHERE `entryorguid`=6784 AND `source_type`=0 AND `id` BETWEEN 4 AND 6;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(6784,0,4,5,61,0,100,0,0,0,0,0,144,1,0,0,0,0,0,1,0,0,0,0,0,0,0,'Calvin Montague - On Link (beaten) - Set Immune To PC On'),
(6784,0,5,0,61,0,100,0,0,0,0,0,22,3,0,0,0,0,0,1,0,0,0,0,0,0,0,'Calvin Montague - On Link - Set Phase 3 (beaten)'),
(6784,0,6,0,21,2,100,0,0,0,0,0,80,678400,2,0,0,0,0,1,0,0,0,0,0,0,0,'Calvin Montague - On Reached Home in the Fight (player died or ran) - Fail the Deal');

DELETE FROM `smart_scripts` WHERE `entryorguid`=678400 AND `source_type`=9;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(678400,9,0,0,0,0,100,0,0,0,0,0,6,590,0,0,0,0,0,12,1,0,0,0,0,0,0,'Calvin Montague - Fail Quest A Rogue''s Deal (VMaNGOS generic_scripts 590)'),
(678400,9,1,0,0,0,100,0,0,0,0,0,2,0,0,0,0,0,0,1,0,0,0,0,0,0,0,'Calvin Montague - Restore Faction'),
(678400,9,2,0,0,0,100,0,0,0,0,0,144,1,0,0,0,0,0,1,0,0,0,0,0,0,0,'Calvin Montague - Set Immune To PC On'),
(678400,9,3,0,0,0,100,0,0,0,0,0,42,0,0,0,0,0,0,1,0,0,0,0,0,0,0,'Calvin Montague - Remove Invincibility'),
(678400,9,4,0,0,0,100,0,0,0,0,0,82,2,0,0,0,0,0,1,0,0,0,0,0,0,0,'Calvin Montague - Add Quest-Giver Flag'),
(678400,9,5,0,0,0,100,0,0,0,0,0,22,1,0,0,0,0,0,1,0,0,0,0,0,0,0,'Calvin Montague - Set Phase 1');

-- Beaten and home: the converted rest scene, plus the success the conversion dropped (VMaNGOS generic_scripts 592: at 10 s the
-- map event ends as a success, completing the quest within 40 yards). The faction comes back with the quest-giver flag.
DELETE FROM `smart_scripts` WHERE `entryorguid`=950001241 AND `source_type`=9;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(950001241,9,0,0,0,0,100,0,2000,2000,0,0,5,14,0,0,0,0,0,1,0,0,0,0,0,0,0,'VMaNGOS generic_scripts 592 - Calvin Montague - Emote Rude'),
(950001241,9,1,0,0,0,100,0,3000,3000,0,0,1,0,0,1,0,0,0,7,0,0,0,0,0,0,0,'VMaNGOS generic_scripts 592 - Calvin Montague - Say Line 0'),
(950001241,9,2,0,0,0,100,0,5000,5000,0,0,11,2639,2,0,0,0,0,1,0,0,0,0,0,0,0,'VMaNGOS generic_scripts 592 - Calvin Montague - Cast Food'),
(950001241,9,3,0,0,0,100,0,0,0,0,0,152,590,0,0,0,0,0,18,40,0,0,0,0,0,0,'VMaNGOS generic_scripts 592/591 - Complete Quest A Rogue''s Deal (40 yards)'),
(950001241,9,4,0,0,0,100,0,0,0,0,0,42,0,0,0,0,0,0,1,0,0,0,0,0,0,0,'Calvin Montague - Remove Invincibility'),
(950001241,9,5,0,0,0,100,0,12000,12000,0,0,82,2,0,0,0,0,0,1,0,0,0,0,0,0,0,'VMaNGOS generic_scripts 592 - Calvin Montague - Add Quest-Giver Flag'),
(950001241,9,6,0,0,0,100,0,0,0,0,0,2,0,0,0,0,0,0,1,0,0,0,0,0,0,0,'Calvin Montague - Restore Faction'),
(950001241,9,7,0,0,0,100,0,0,0,0,0,22,1,0,0,0,0,0,1,0,0,0,0,0,0,0,'VMaNGOS generic_scripts 592 - Calvin Montague - Set Phase 1'),
(950001241,9,8,0,0,0,100,0,8000,8000,0,0,90,0,0,0,0,0,0,1,0,0,0,0,0,0,0,'VMaNGOS generic_scripts 592 - Calvin Montague - Stand Up');
