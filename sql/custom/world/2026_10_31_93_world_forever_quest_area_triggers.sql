-- Two quests whose only objective is "enter an area trigger" (quest_objectives Type 19) had no server-side area trigger with that
-- id, so the objective could never complete:
--   A Frightened Request (92401, Tabitha Heartweaver, Ruins of Lordaeron): objective 464558 = area trigger 39396.
--     Placed as a 12 yard sphere on Edward Heartweaver (250842), the only spawn the quest text points to (map 2999).
--   Secure the Mountain (98319, Mountaineer Gretchen, Dun Morogh): objective 477368 "Mountaineer Cornelius found" = 44117.
--     Placed as a polygon over the client's own quest map area for that objective (quest_poi_points 98319 blob 1, 12 points)
--     around the Grizzled Den, 16 yards up and down from z 360 (the wendigos in the den stand at z 347-372; the hill above is 397+).
-- Same layout as the Turncoat trigger (2026_10_27_15). The core credits Type 19 objectives with the trigger's own id on entry, so
-- no script is needed. Server-side flag set, so the client is never sent a trigger it does not know.
DELETE FROM `areatrigger_template` WHERE `Id` IN (39396, 44117) AND `IsCustom` = 1;
INSERT INTO `areatrigger_template` (`Id`, `IsCustom`, `Flags`, `ActionSetId`, `ActionSetFlags`, `VerifiedBuild`) VALUES
(39396, 1, 1, 0, 0, 0),
(44117, 1, 1, 0, 0, 0);
DELETE FROM `areatrigger_create_properties` WHERE `Id` IN (39396, 44117) AND `IsCustom` = 0;
INSERT INTO `areatrigger_create_properties` (`Id`, `IsCustom`, `AreaTriggerId`, `IsAreatriggerCustom`, `Flags`, `AnimId`, `AnimKitId`,
    `PositionalSoundKitId`, `Speed`, `Shape`, `ShapeData0`, `ShapeData1`, `ShapeData2`, `ShapeData3`, `ShapeData4`, `ShapeData5`,
    `ShapeData6`, `ShapeData7`, `Roll`, `Pitch`, `Yaw`, `ScriptName`, `VerifiedBuild`) VALUES
(39396, 0, 39396, 1, 0, -1, -1, 0, 0, 0, 12, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, '', 0),
(44117, 0, 44117, 1, 0, -1, -1, 0, 0, 3, 16, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, '', 0);
DELETE FROM `areatrigger_create_properties_polygon_vertex` WHERE `AreaTriggerCreatePropertiesId` = 44117 AND `IsCustom` = 0;
INSERT INTO `areatrigger_create_properties_polygon_vertex` (`AreaTriggerCreatePropertiesId`, `IsCustom`, `Idx`, `VerticeX`, `VerticeY`, `VerticeTargetX`, `VerticeTargetY`, `VerifiedBuild`) VALUES
(44117, 0, 0, 73.5, -128, NULL, NULL, 0),
(44117, 0, 1, 107.5, -100, NULL, NULL, 0),
(44117, 0, 2, 138.5, -21, NULL, NULL, 0),
(44117, 0, 3, 144.5, 39, NULL, NULL, 0),
(44117, 0, 4, 95.5, 129, NULL, NULL, 0),
(44117, 0, 5, 29.5, 153, NULL, NULL, 0),
(44117, 0, 6, -30.5, 144, NULL, NULL, 0),
(44117, 0, 7, -126.5, 81, NULL, NULL, 0),
(44117, 0, 8, -158.5, 7, NULL, NULL, 0),
(44117, 0, 9, -153.5, -46, NULL, NULL, 0),
(44117, 0, 10, -105.5, -108, NULL, NULL, 0),
(44117, 0, 11, -15.5, -151, NULL, NULL, 0);
DELETE FROM `areatrigger` WHERE `SpawnId` IN (9201, 9202);
INSERT INTO `areatrigger` (`SpawnId`, `AreaTriggerCreatePropertiesId`, `IsCustom`, `MapId`, `SpawnDifficulties`, `PosX`, `PosY`, `PosZ`, `Orientation`,
    `PhaseUseFlags`, `PhaseId`, `PhaseGroup`, `ScriptName`, `Comment`, `VerifiedBuild`) VALUES
(9201, 39396, 0, 2999, '1', 1524.31, 289.026, 61.6613, 0, 0, 0, 0, '', 'Ruins of Lordaeron - Edward Heartweaver (quest 92401 A Frightened Request)', 0),
(9202, 44117, 0, 0, '0', -5494.5, -258, 360, 0, 0, 0, 0, '', 'Dun Morogh - Grizzled Den (quest 98319 Secure the Mountain)', 0);
