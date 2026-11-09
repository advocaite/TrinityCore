-- Classic 1.60: Nefarian phase 3 (Bone Constructs) can happen.
-- Vanilla chain (VMaNGOS 20200212185807_world.sql, 1.60 client spell data checked): a dying drakonid casts 23363 "Summon
-- Drakonid Corpse Trigger" (summons object 179804 Drakonid Bones, a one-charge trap holding 23361 "Raise Undead Drakonid");
-- Nefarian under 20 percent casts 23362 "Raise Drakonids" (disturb the bones in range), each trap then raises one 14605
-- Bone Construct and removes itself. Our tree had the Nefarian cast and the Bone Construct AI but no 179804 template, no
-- on-death cast on the five coloured drakonids (only Chromatic 14302 had it) and no target condition on 23362.

-- 1. Drakonid Bones: vanilla trap data (type 6, spell 23361, one charge). The display id is not in any source available
--    here; a live row keeps its own display id, a new row shows no bone pile until it is filled in.
INSERT INTO `gameobject_template` (`entry`, `type`, `displayId`, `name`, `size`, `Data3`, `Data4`, `VerifiedBuild`) VALUES
(179804, 6, 0, 'Drakonid Bones', 1, 23361, 1, 0)
ON DUPLICATE KEY UPDATE `type` = 6, `Data3` = 23361, `Data4` = 1;

-- 2. Raise Drakonids 23362 (area object activation) may only disturb Drakonid Bones, not every object in the lair.
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 13 AND `SourceEntry` = 23362;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`, `SourceGroup`, `SourceEntry`, `SourceId`, `ElseGroup`, `ConditionTypeOrReference`, `ConditionTarget`, `ConditionValue1`, `ConditionValue2`, `ConditionValue3`, `ConditionStringValue1`, `NegativeCondition`, `ErrorType`, `ErrorTextId`, `ScriptName`, `Comment`) VALUES
(13, 1, 23362, 0, 0, 31, 0, 5, 179804, 0, '', 0, 0, 0, '', 'Raise Drakonids - only Drakonid Bones');

-- 3. Blue / Green / Bronze / Red / Black Drakonid (14261-14265) leave their bones like the Chromatic Drakonid does
--    (same rows as 14302 in 2026_10_11_24: on death cast 23363 triggered, then despawn).
UPDATE `creature_template` SET `AIName` = 'SmartAI', `ScriptName` = '' WHERE `entry` IN (14261, 14262, 14263, 14264, 14265);
DELETE FROM `smart_scripts` WHERE `entryorguid` IN (14261, 14262, 14263, 14264, 14265) AND `source_type` = 0;
INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`, `Difficulties`, `event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `event_param_string`, `action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`, `action_param7`, `action_param_string`, `target_type`, `target_param1`, `target_param2`, `target_param3`, `target_param4`, `target_param_string`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
(14261, 0, 0, 1, '', 6, 0, 100, 1, 0, 0, 0, 0, 0, '', 11, 23363, 3, 0, 0, 0, 0, 0, '', 1, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Blue Drakonid - On Death - Cast ''Summon Drakonid Corpse Trigger'''),
(14261, 0, 1, 0, '', 61, 0, 100, 0, 0, 0, 0, 0, 0, '', 41, 0, 0, 0, 0, 0, 0, 0, '', 1, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Blue Drakonid - On Death - Despawn'),
(14262, 0, 0, 1, '', 6, 0, 100, 1, 0, 0, 0, 0, 0, '', 11, 23363, 3, 0, 0, 0, 0, 0, '', 1, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Green Drakonid - On Death - Cast ''Summon Drakonid Corpse Trigger'''),
(14262, 0, 1, 0, '', 61, 0, 100, 0, 0, 0, 0, 0, 0, '', 41, 0, 0, 0, 0, 0, 0, 0, '', 1, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Green Drakonid - On Death - Despawn'),
(14263, 0, 0, 1, '', 6, 0, 100, 1, 0, 0, 0, 0, 0, '', 11, 23363, 3, 0, 0, 0, 0, 0, '', 1, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Bronze Drakonid - On Death - Cast ''Summon Drakonid Corpse Trigger'''),
(14263, 0, 1, 0, '', 61, 0, 100, 0, 0, 0, 0, 0, 0, '', 41, 0, 0, 0, 0, 0, 0, 0, '', 1, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Bronze Drakonid - On Death - Despawn'),
(14264, 0, 0, 1, '', 6, 0, 100, 1, 0, 0, 0, 0, 0, '', 11, 23363, 3, 0, 0, 0, 0, 0, '', 1, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Red Drakonid - On Death - Cast ''Summon Drakonid Corpse Trigger'''),
(14264, 0, 1, 0, '', 61, 0, 100, 0, 0, 0, 0, 0, 0, '', 41, 0, 0, 0, 0, 0, 0, 0, '', 1, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Red Drakonid - On Death - Despawn'),
(14265, 0, 0, 1, '', 6, 0, 100, 1, 0, 0, 0, 0, 0, '', 11, 23363, 3, 0, 0, 0, 0, 0, '', 1, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Black Drakonid - On Death - Cast ''Summon Drakonid Corpse Trigger'''),
(14265, 0, 1, 0, '', 61, 0, 100, 0, 0, 0, 0, 0, 0, '', 41, 0, 0, 0, 0, 0, 0, 0, '', 1, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Black Drakonid - On Death - Despawn'),
-- 4. Their Brood Power (VMaNGOS creature_spells 142610-142650: Blue 22559, Green 22561, Bronze 22642, Red 22558, Black 22560;
--    timing as the Chromatic Drakonid list 143020, the only one of the six whose timers are in our data).
(14261, 0, 2, 0, '', 0, 0, 100, 0, 10000, 10000, 4000, 6000, 0, '', 11, 22559, 0, 0, 0, 0, 0, 0, '', 2, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Blue Drakonid - In Combat - Cast ''Brood Power: Blue'''),
(14262, 0, 2, 0, '', 0, 0, 100, 0, 10000, 10000, 4000, 6000, 0, '', 11, 22561, 0, 0, 0, 0, 0, 0, '', 2, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Green Drakonid - In Combat - Cast ''Brood Power: Green'''),
(14263, 0, 2, 0, '', 0, 0, 100, 0, 10000, 10000, 4000, 6000, 0, '', 11, 22642, 0, 0, 0, 0, 0, 0, '', 2, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Bronze Drakonid - In Combat - Cast ''Brood Power: Bronze'''),
(14264, 0, 2, 0, '', 0, 0, 100, 0, 10000, 10000, 4000, 6000, 0, '', 11, 22558, 0, 0, 0, 0, 0, 0, '', 2, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Red Drakonid - In Combat - Cast ''Brood Power: Red'''),
(14265, 0, 2, 0, '', 0, 0, 100, 0, 10000, 10000, 4000, 6000, 0, '', 11, 22560, 0, 0, 0, 0, 0, 0, '', 2, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Black Drakonid - In Combat - Cast ''Brood Power: Black''');
