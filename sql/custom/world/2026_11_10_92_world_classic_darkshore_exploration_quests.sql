-- Darkshore exploration quests a player reported (Discord, 2026-11-10): The Red Crystal (4811) and How Big a Threat? (984)
-- showed as complete right after accepting, with no map point, and could not be turned in.
-- Both are "go there" quests (quest flag 4, completed by an area trigger). The client has no objective to show for such a
-- quest, so it looks complete at once; the server only lets it be turned in once the player has been there. No trigger
-- credited either quest, so they could never be turned in.
--   The Red Crystal: vanilla trigger 2486 (the red crystal, 15 yards) is still in the 1.60 client AreaTrigger data, so it only
--     needed its areatrigger_involvedrelation row.
--   How Big a Threat?: its vanilla triggers 231, 232 (the furbolg camp, 40 yards) and 235 (70 yards) are gone from the client
--     data, so they come back as server-side area triggers at the vanilla spots (vanilla positions and radii) with a SmartAI
--     script that credits the quest on entry (SMART_ACTION_COMPLETE_QUEST, only for quests with flag 4). Server-side flag
--     set, so the client is never sent a trigger it does not know (same as 2026_10_31_93).
-- Safe to run again.

DELETE FROM `areatrigger_involvedrelation` WHERE `id` = 2486 AND `quest` = 4811;
INSERT INTO `areatrigger_involvedrelation` (`id`,`quest`) VALUES (2486,4811);

DELETE FROM `areatrigger_template` WHERE `Id` = 9840 AND `IsCustom` = 1;
INSERT INTO `areatrigger_template` (`Id`, `IsCustom`, `Flags`, `ActionSetId`, `ActionSetFlags`, `VerifiedBuild`) VALUES
(9840, 1, 1, 0, 0, 0);
DELETE FROM `areatrigger_create_properties` WHERE `Id` IN (9840, 9841) AND `IsCustom` = 1;
INSERT INTO `areatrigger_create_properties` (`Id`, `IsCustom`, `AreaTriggerId`, `IsAreatriggerCustom`, `Flags`, `AnimId`, `AnimKitId`,
    `PositionalSoundKitId`, `Speed`, `Shape`, `ShapeData0`, `ShapeData1`, `ShapeData2`, `ShapeData3`, `ShapeData4`, `ShapeData5`,
    `ShapeData6`, `ShapeData7`, `Roll`, `Pitch`, `Yaw`, `ScriptName`, `VerifiedBuild`) VALUES
(9840, 1, 9840, 1, 0, -1, -1, 0, 0, 0, 40, 40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 'SmartAreaTriggerAI', 0),
(9841, 1, 9840, 1, 0, -1, -1, 0, 0, 0, 70, 70, 0, 0, 0, 0, 0, 0, 0, 0, 0, 'SmartAreaTriggerAI', 0);
DELETE FROM `areatrigger` WHERE `SpawnId` IN (9203, 9204, 9205);
INSERT INTO `areatrigger` (`SpawnId`, `AreaTriggerCreatePropertiesId`, `IsCustom`, `MapId`, `SpawnDifficulties`, `PosX`, `PosY`, `PosZ`, `Orientation`,
    `PhaseUseFlags`, `PhaseId`, `PhaseGroup`, `ScriptName`, `Comment`, `VerifiedBuild`) VALUES
(9203, 9840, 1, 1, '0', 5996.56, 365.21, 21.8661, 0, 0, 0, 0, '', 'Darkshore - furbolg camp (quest 984 How Big a Threat?, vanilla trigger 231)', 0),
(9204, 9840, 1, 1, '0', 5980.17, 330.669, 21.7655, 0, 0, 0, 0, '', 'Darkshore - furbolg camp (quest 984 How Big a Threat?, vanilla trigger 232)', 0),
(9205, 9841, 1, 1, '0', 4554.65, 146.354, 59.5027, 0, 0, 0, 0, '', 'Darkshore - furbolg camp (quest 984 How Big a Threat?, vanilla trigger 235)', 0);

DELETE FROM `smart_scripts` WHERE `entryorguid` = 9840 AND `source_type` = 12;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`Difficulties`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`event_param_string`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`action_param7`,`action_param_string`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_param4`,`target_param_string`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(9840,12,0,0,'',46,0,100,0,0,0,0,0,0,'',152,984,0,0,0,0,0,0,'',7,0,0,0,0,'',0,0,0,0,'Furbolg camp area trigger - On enter - Complete How Big a Threat? (984) for the player');
