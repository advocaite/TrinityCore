-- "Secure the Mountain" (98319) objective "Mountaineer Cornelius found" is QUEST_OBJECTIVE_AREA_TRIGGER_ENTER on areatrigger
-- 44117, which did not exist, so the quest could never progress. Mountaineer Cornelius himself is a decoration object
-- (gameobject 672497, type 5, not clickable). Server-side sphere (8 yd) on his body in the Grizzled Den.
START TRANSACTION;

DELETE FROM `areatrigger_template` WHERE `Id` = 44117 AND `IsCustom` = 0;
INSERT INTO `areatrigger_template` (`Id`, `IsCustom`, `Flags`, `ActionSetId`, `ActionSetFlags`, `VerifiedBuild`) VALUES
(44117, 0, 1, 0, 0, 0);

DELETE FROM `areatrigger_create_properties` WHERE `Id` = 1310000 AND `IsCustom` = 1;
INSERT INTO `areatrigger_create_properties` (`Id`, `IsCustom`, `AreaTriggerId`, `IsAreatriggerCustom`, `Shape`, `ShapeData0`, `ShapeData1`, `VerifiedBuild`) VALUES
(1310000, 1, 44117, 0, 0, 8, 8, 0);

DELETE FROM `areatrigger` WHERE `SpawnId` = 1310000;
INSERT INTO `areatrigger` (`SpawnId`, `AreaTriggerCreatePropertiesId`, `IsCustom`, `MapId`, `SpawnDifficulties`, `PosX`, `PosY`, `PosZ`, `Orientation`, `PhaseUseFlags`, `PhaseId`, `PhaseGroup`, `ScriptName`, `Comment`, `VerifiedBuild`) VALUES
(1310000, 1310000, 1, 0, '0', -5506.33, -317.141, 352.31, 0, 1, 0, 0, '', 'Dun Morogh - Grizzled Den - Mountaineer Cornelius found (quest 98319)', 0);
COMMIT;
