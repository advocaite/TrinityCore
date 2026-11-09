-- Wailing Caverns entrance (Barrens): Deviate Creeper 3632 and Deviate Stalker 3634, the "deviate lizards" (raptor family),
-- could not be attacked by bots or hunter pets, while the Deviate Coilers (wind serpents) next to them could.
-- Discord 11-08 (fox4039).
--
-- Cause: these two raptors are the only mobs there that sneak: 3632 casts Sneak 22766 when it spawns (VMaNGOS EventAI),
-- 3634 wears Faded 6408 (template aura). Both are plain creature stealth and stay on in combat with our client data, so
-- every attack needs the attacker to detect stealth: only in front of the unit and within about 30 yd of it. A player who
-- turns to the mob can hit it; a bot standing behind or beside its owner, a pet whose owner looks away, or anyone
-- further away loses the target and stops.
-- Vanilla stealthed mobs show themselves once they fight. Same here: the stealth aura comes off when the raptor enters
-- combat and goes back on when it is home again after leaving combat.
-- Safe to run again.
UPDATE `creature_template` SET `AIName`='SmartAI' WHERE `entry`=3634 AND `AIName`='' AND `ScriptName`='';
DELETE FROM `smart_scripts` WHERE `entryorguid` IN (3632,3634) AND `source_type`=0 AND `id` IN (10,11);
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_param4`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(3632,0,10,0,4,0,100,0,0,0,0,0,0,28,22766,0,0,0,0,0,1,0,0,0,0,0,0,0,0,'Deviate Creeper - On Aggro - Remove Sneak (attackable by bots and pets in combat)'),
(3632,0,11,0,21,0,100,0,0,0,0,0,0,11,22766,2,0,0,0,0,1,0,0,0,0,0,0,0,0,'Deviate Creeper - On Reached Home - Cast Sneak again'),
(3634,0,10,0,4,0,100,0,0,0,0,0,0,28,6408,0,0,0,0,0,1,0,0,0,0,0,0,0,0,'Deviate Stalker - On Aggro - Remove Faded (attackable by bots and pets in combat)'),
(3634,0,11,0,21,0,100,0,0,0,0,0,0,11,6408,2,0,0,0,0,1,0,0,0,0,0,0,0,0,'Deviate Stalker - On Reached Home - Cast Faded again');
