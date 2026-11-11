-- Classic 1.60 (WoW Forever): paladin Redemption quests, Symbol of Life (item 6866, spell 8593). Safe to run again.
--
-- The Symbol of Life spell picks its target by creature entry. The base DB lists Henze Faulk (human chain, quest 1786)
-- and Narm Faulk (dwarf chain, quest 1783) but not Deathguard Falgan, the Forever undead chain (A Lesson in Divinity 94438).
-- Without a row the Symbol only works with Falgan already targeted. All rows are written here so the set is complete.
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 13 AND `SourceEntry` = 8593;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`, `SourceGroup`, `SourceEntry`, `SourceId`, `ElseGroup`, `ConditionTypeOrReference`, `ConditionTarget`,
`ConditionValue1`, `ConditionValue2`, `ConditionValue3`, `NegativeCondition`, `Comment`) VALUES
(13, 1, 8593, 0, 0, 31, 0, 3, 17542, 0, 0, 'Symbol of Life can be cast on Young Furbolg Shaman'),
(13, 1, 8593, 0, 1, 31, 0, 3, 6172, 0, 0, 'Symbol of Life can be cast on Henze Faulk'),
(13, 1, 8593, 0, 2, 31, 0, 3, 6177, 0, 0, 'Symbol of Life can be cast on Narm Faulk'),
(13, 1, 8593, 0, 3, 31, 0, 3, 257663, 0, 0, 'Symbol of Life can be cast on Deathguard Falgan');

-- Deathguard Falgan lies dead at Venomweb Vale until a paladin uses the Symbol of Life on him (sniff: npcflag 0 while dead),
-- like Henze and Narm Faulk. He gets up, and after two minutes lies down again for the next paladin.
INSERT INTO `creature_template_addon` (`entry`, `StandState`) VALUES (257663, 7)
ON DUPLICATE KEY UPDATE `StandState` = VALUES(`StandState`);

UPDATE `creature_template` SET `AIName` = 'SmartAI', `ScriptName` = '' WHERE `entry` = 257663;
DELETE FROM `smart_scripts` WHERE `entryorguid` = 257663 AND `source_type` = 0;
INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`, `event_type`, `event_phase_mask`, `event_chance`, `event_flags`,
`event_param1`, `event_param2`, `event_param3`, `event_param4`, `action_type`, `action_param1`, `action_param2`, `action_param3`,
`target_type`, `target_param1`, `comment`) VALUES
(257663, 0, 0, 1, 8, 0, 100, 0, 8593, 0, 0, 0, 91, 0, 0, 0, 1, 0, 'Deathguard Falgan - On Symbol of Life hit - Stand up'),
(257663, 0, 1, 0, 61, 0, 100, 0, 0, 0, 0, 0, 41, 120000, 5, 0, 1, 0, 'Deathguard Falgan - Linked - Lie down again after 2 minutes');
