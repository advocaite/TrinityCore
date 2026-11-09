-- Classic 1.60: Alterac Valley NPC AI from VMaNGOS 1.12 (battleground_alterac.cpp and its EventAI). Spells checked against 70532.
--
-- 1. Stormpike Bowman 13358 and Frostwolf Bowman 13359 (the first tier tower archers) never fought. 2026_09_28_15 converted their
--    "Enter Evade Mode" EventAI to a plain in combat timer, but VMaNGOS runs that script only under condition 13358 (victim more
--    than 81 yards away). So they evaded one second into every fight. The row now fires on SMART_EVENT_RANGE 81+ yards.
-- 2. The higher tier archers (Seasoned, Veteran, Champion Outrunner and Ranger, 96 spawns) had no AI at all. They get the C++
--    npc_av_bowman (VMaNGOS npc_AlteracBowman): rooted, Shoot 22121 at anything hostile in sight up to 80 yards.
-- 3. Marshals and Warmasters 14762-14777: half of them had no script. All 16 use npc_av_marshal_or_warmaster (C++, now with the
--    VMaNGOS timers and the 70532 Cleave 20684).
-- 4. Mine layers 13356/13357 and explosives experts 13597/13598: npc_av_mine_layer (VMaNGOS MineNPC_AI): Bomb, Flash Bomb,
--    Landmine, and the whole area joins the fight.
-- 5. Korrak the Bloodrager 12159: npc_av_korrak_the_bloodrager (VMaNGOS): he joins only after two hours of battle, with his yell.
-- 6. Trigger Guse, Mulverick, Jeztor, Ichman, Slidore and Vipore 14026-14031 (quests 7301/7302 "Fallen Sky Lords", one credit for
--    each crash site): VMaNGOS npc_av_trigger_for_quest gives the kill credit to a player within 10 yards. Nothing gave it here,
--    so neither quest could be finished.
-- Safe to run again.

UPDATE `smart_scripts` SET `event_type` = 9, `event_param1` = 81, `event_param2` = 5000, `event_param3` = 1000, `event_param4` = 1000
    WHERE `source_type` = 0 AND `entryorguid` IN (13358,13359) AND `id` = 4 AND `action_type` = 24;

UPDATE `creature_template` SET `AIName` = '', `ScriptName` = 'npc_av_bowman' WHERE `entry` IN (13516,13517,13518,13519,13520,13521,13522,13523);
UPDATE `creature_template` SET `AIName` = '', `ScriptName` = 'npc_av_marshal_or_warmaster' WHERE `entry` BETWEEN 14762 AND 14777;
UPDATE `creature_template` SET `AIName` = '', `ScriptName` = 'npc_av_mine_layer' WHERE `entry` IN (13356,13357,13597,13598);
UPDATE `creature_template` SET `AIName` = '', `ScriptName` = 'npc_av_korrak_the_bloodrager' WHERE `entry` = 12159;

UPDATE `creature_template` SET `AIName` = 'SmartAI', `ScriptName` = '' WHERE `entry` BETWEEN 14026 AND 14031;
DELETE FROM `smart_scripts` WHERE `source_type` = 0 AND `entryorguid` BETWEEN 14026 AND 14031;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`Difficulties`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`event_param_string`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`action_param7`,`action_param_string`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_param4`,`target_param_string`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(14026,0,0,0,'',10,0,100,0,2,10,0,0,1,'',33,14026,0,0,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'Trigger Guse - player within 10 yd - kill credit (VMaNGOS npc_av_trigger_for_quest)'),
(14027,0,0,0,'',10,0,100,0,2,10,0,0,1,'',33,14027,0,0,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'Trigger Mulverick - player within 10 yd - kill credit (VMaNGOS npc_av_trigger_for_quest)'),
(14028,0,0,0,'',10,0,100,0,2,10,0,0,1,'',33,14028,0,0,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'Trigger Jeztor - player within 10 yd - kill credit (VMaNGOS npc_av_trigger_for_quest)'),
(14029,0,0,0,'',10,0,100,0,2,10,0,0,1,'',33,14029,0,0,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'Trigger Ichman - player within 10 yd - kill credit (VMaNGOS npc_av_trigger_for_quest)'),
(14030,0,0,0,'',10,0,100,0,2,10,0,0,1,'',33,14030,0,0,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'Trigger Slidore - player within 10 yd - kill credit (VMaNGOS npc_av_trigger_for_quest)'),
(14031,0,0,0,'',10,0,100,0,2,10,0,0,1,'',33,14031,0,0,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'Trigger Vipore - player within 10 yd - kill credit (VMaNGOS npc_av_trigger_for_quest)');
