-- Classic 1.60: five dungeon creatures still carried stock TrinityCore ScriptNames. Those AIs are created only
-- when the map runs the stock instance script, so on the classic instance scripts they fell back to plain melee.
-- Tuten'kash (7355) and Death Knight Darkreaver (14516) get the stock spell sets as SmartAI; the Tomb Fiend,
-- Tomb Reaver (7349, 7351) and Grubbis (7361) scripts only tracked stock instance data and become plain SmartAI.
UPDATE `creature_template` SET `ScriptName` = '', `AIName` = 'SmartAI' WHERE `entry` IN (7349, 7351, 7355, 7361, 14516);
DELETE FROM `smart_scripts` WHERE `entryorguid` IN (7355, 14516) AND `source_type` = 0;
INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`, `Difficulties`, `event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `event_param_string`, `action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`, `action_param7`, `action_param_string`, `target_type`, `target_param1`, `target_param2`, `target_param3`, `target_param4`, `target_param_string`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
(7355, 0, 0, 0, '', 25, 0, 100, 0, 0, 0, 0, 0, 0, '', 11, 12254, 2, 0, 0, 0, 0, 0, '', 1, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Tuten''kash - On Reset - Cast ''Virulent Poison'''),
(7355, 0, 1, 0, '', 25, 0, 100, 0, 0, 0, 0, 0, 0, '', 11, 8876, 2, 0, 0, 0, 0, 0, '', 1, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Tuten''kash - On Reset - Cast ''Thrash'''),
(7355, 0, 2, 0, '', 0, 0, 100, 0, 10000, 20000, 15000, 25000, 0, '', 11, 12252, 0, 0, 0, 0, 0, 0, '', 1, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Tuten''kash - In Combat - Cast ''Web Spray'''),
(7355, 0, 3, 0, '', 0, 0, 100, 0, 10000, 15000, 15000, 25000, 0, '', 11, 12255, 0, 0, 0, 0, 0, 0, '', 1, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Tuten''kash - In Combat - Cast ''Curse of Tuten''kash'''),
(14516, 0, 0, 0, '', 0, 0, 100, 0, 0, 0, 5000, 10000, 0, '', 11, 17393, 0, 0, 0, 0, 0, 0, '', 2, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Death Knight Darkreaver - In Combat - Cast ''Shadow Bolt'''),
(14516, 0, 1, 0, '', 0, 0, 100, 0, 5000, 10000, 5000, 10000, 0, '', 11, 15284, 0, 0, 0, 0, 0, 0, '', 2, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Death Knight Darkreaver - In Combat - Cast ''Cleave'''),
(14516, 0, 2, 0, '', 0, 0, 100, 0, 10000, 15000, 15000, 25000, 0, '', 11, 22644, 0, 0, 0, 0, 0, 0, '', 1, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Death Knight Darkreaver - In Combat - Cast ''Blood Leech'''),
(14516, 0, 3, 0, '', 0, 0, 100, 0, 10000, 15000, 15000, 20000, 0, '', 11, 7645, 0, 0, 0, 0, 0, 0, '', 6, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Death Knight Darkreaver - In Combat - Cast ''Dominate Mind'' on a random non-tank'),
(14516, 0, 4, 0, '', 6, 0, 100, 0, 0, 0, 0, 0, 0, '', 11, 23261, 2, 0, 0, 0, 0, 0, '', 1, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Death Knight Darkreaver - On Death - Cast ''Summon Fallen Charger''');
