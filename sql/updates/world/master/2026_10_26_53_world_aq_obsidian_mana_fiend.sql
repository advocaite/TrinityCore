-- VMaNGOS 46417908 (AQ20 Kurinnaxx, Obsidian Destroyer, Moam and AQ40 Obsidian Eradicator, sniff based).
-- The C++ side (Kurinnaxx, Moam, Obsidian Destroyer) is in the classic Ruins of Ahn'Qiraj scripts.

-- Obsidian Eradicator 15262: mana reset (23777) on spawn, on reaching home and on aggro, so Shock Blast waits
-- for the mana it drains; Drop Obsidian (27628, Small Obsidian Chunk 181068) on death, which it never did here.
DELETE FROM `smart_scripts` WHERE `entryorguid`=15262 AND `source_type`=0 AND `id` BETWEEN 3 AND 6;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(15262,0,3,0,11,0,100,0,0,0,0,0,0,11,23777,2,0,0,0,0,1,0,0,0,0,0,0,0,'Obsidian Eradicator - On Spawn - Cast Zero Mana/Full Health DND (VMaNGOS EventAI 1526200)'),
(15262,0,4,0,21,0,100,0,0,0,0,0,0,11,23777,2,0,0,0,0,1,0,0,0,0,0,0,0,'Obsidian Eradicator - On Reached Home - Cast Zero Mana/Full Health DND (VMaNGOS EventAI 1526201)'),
(15262,0,5,0,4,0,100,0,0,0,0,0,0,11,23777,2,0,0,0,0,1,0,0,0,0,0,0,0,'Obsidian Eradicator - On Aggro - Cast Zero Mana/Full Health DND (VMaNGOS EventAI 1526203)'),
(15262,0,6,0,6,0,100,0,0,0,0,0,0,11,27628,2,0,0,0,0,1,0,0,0,0,0,0,0,'Obsidian Eradicator - On Death - Cast Drop Obsidian (VMaNGOS EventAI 1526202)');

-- Shock Blast at full mana as in VMaNGOS (was 99%)
UPDATE `smart_scripts` SET `event_param1`=100, `comment`='Obsidian Eradicator - At Full Mana - Cast Shock Blast (VMaNGOS EventAI 1526204)'
WHERE `entryorguid`=15262 AND `source_type`=0 AND `id`=0;

-- Mana Fiend 15527 (Moam): Arcane Explosion 6 seconds after the summon, all three in step, then every 6 seconds;
-- Counterspell on a casting target every 11 seconds (VMaNGOS spell list 155270).
UPDATE `smart_scripts` SET `event_param1`=6000, `event_param2`=6000, `event_param3`=6000, `event_param4`=6000,
`comment`='Mana Fiend - In Combat - Cast Arcane Explosion (VMaNGOS spell list 155270)'
WHERE `entryorguid`=15527 AND `source_type`=0 AND `id`=2;
UPDATE `smart_scripts` SET `event_param1`=11000, `event_param2`=11000,
`comment`='Mana Fiend - Target Casting - Cast Counterspell (VMaNGOS spell list 155270)'
WHERE `entryorguid`=15527 AND `source_type`=0 AND `id`=3;
