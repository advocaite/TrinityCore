-- Boss mechanics our vMaNGOS based scripts lack, cross-checked against cMaNGOS classic (and TC where it has a script).

-- Magmadar 11982: Magma Spit (19449, proc aura: melee hits add 19450 fire damage and a burn).
-- TC boss_magmadar and cMaNGOS both put it on him; our SmartAI port never did. TC reapplies template auras on
-- spawn and after evade.
INSERT INTO `creature_template_addon` (`entry`,`auras`) VALUES (11982,'19449')
ON DUPLICATE KEY UPDATE `auras`='19449';

-- Lord Kazzak 12397: the three minute enrage only yelled. cMaNGOS casts Berserk (21340: 75% haste and a Shadow Bolt
-- Volley every 2 seconds); VMaNGOS switches to a spell list with the volley every second.
UPDATE `smart_scripts` SET `link`=11 WHERE `entryorguid`=12397 AND `source_type`=0 AND `id`=3;
DELETE FROM `smart_scripts` WHERE `entryorguid`=12397 AND `source_type`=0 AND `id`=11;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(12397,0,11,0,61,0,100,0,0,0,0,0,0,11,21340,2,0,0,0,0,1,0,0,0,0,0,0,0,'Lord Kazzak - After 3 Minutes - Cast Berserk (cMaNGOS boss_kazzak)');

-- Ambassador Flamelash 9156: Burning Spirit (13489) is cast by a spirit that reaches him; its dummy effect has to
-- hit Flamelash (VMaNGOS spell_script_target 13489 -> 9156), whose SmartAI then casts 14744. The row pointed at
-- the spirits themselves.
UPDATE `conditions` SET `ConditionValue2`=9156, `Comment`='Burning Spirit - target Ambassador Flamelash'
WHERE `SourceTypeOrReferenceId`=13 AND `SourceGroup`=2 AND `SourceEntry`=13489 AND `ConditionTypeOrReference`=31 AND `ConditionValue2`=9178;
