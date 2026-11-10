-- Rows that three update files of 26 Oct were meant to give every world, and that fresh 0.1.30.1 installs do not have
-- (dev realm compared with the 0.1.30.1 package database, 6 Nov): sql/updates/world/master 2026_10_26_53 (Obsidian Eradicator,
-- Mana Fiend), 2026_10_26_54 (Magmadar, Lord Kazzak, Ambassador Flamelash) and 2026_10_26_56 (Touch of the Grave). Players missed:
-- the Undead racial Touch of the Grave never drained (no script on 1260189), Magmadar had no Magma Spit, Kazzak's enrage only
-- yelled, the Obsidian Eradicator never reset its mana or dropped its obsidian, Mana Fiends kept the old timers.
-- The same statements again, as a custom file so every install runs it. Each one deletes and writes its own key, or sets
-- fixed values, so it is safe to run again and gives the same rows on a world that already had the three files.

-- From 2026_10_26_53 (VMaNGOS 46417908, sniff based)
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

-- From 2026_10_26_54 (cross-checked against cMaNGOS classic)
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

-- From 2026_10_26_56
-- Touch of the Grave (Undead racial 1260189): the dummy proc aura now casts the drain 1260198 for $s1% of maximum health
DELETE FROM `spell_script_names` WHERE `spell_id` = 1260189 AND `ScriptName` = 'classic_spell_undead_touch_of_the_grave';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(1260189, 'classic_spell_undead_touch_of_the_grave');
