-- Classic 1.60: Hurricane (16914, 17401, 17402). Same layout as Blizzard: effect 1 creates an area trigger (create properties 41199 /
-- 41198 / 41170, 8 yd sphere as the client radius) and effect 2 is a periodic dummy aura on the caster that nothing ticked. The tick
-- damage is classic_spell_ground_area_damage (C++: Custom/Classic/world/classic_spell_scripts.cpp). Safe to run again.
DELETE FROM `areatrigger_template` WHERE `Id` IN (41199,41198,41170) AND `IsCustom` = 1;
INSERT INTO `areatrigger_template` (`Id`, `IsCustom`, `Flags`, `ActionSetId`, `ActionSetFlags`, `VerifiedBuild`) VALUES
(41199, 1, 0, 0, 0, 0),
(41198, 1, 0, 0, 0, 0),
(41170, 1, 0, 0, 0, 0);

DELETE FROM `areatrigger_create_properties` WHERE `Id` IN (41199,41198,41170) AND `IsCustom` = 0;
INSERT INTO `areatrigger_create_properties` (`Id`, `IsCustom`, `AreaTriggerId`, `IsAreatriggerCustom`, `Flags`, `MoveCurveId`,
`ScaleCurveId`, `MorphCurveId`, `FacingCurveId`, `AnimId`, `AnimKitId`, `DecalPropertiesId`, `SpellForVisuals`, `PositionalSoundKitId`,
`TimeToTargetScale`, `Speed`, `SpeedIsTime`, `Shape`, `ShapeData0`, `ShapeData1`, `ShapeData2`, `ShapeData3`, `ShapeData4`,
`ShapeData5`, `ShapeData6`, `ShapeData7`, `Roll`, `Pitch`, `Yaw`, `TargetRoll`, `TargetPitch`, `TargetYaw`, `ScriptName`,
`VerifiedBuild`) VALUES
(41199, 0, 41199, 1, 0, 0, 0, 0, 0, -1, -1, 0, NULL, 0, 0, 0, 0, 0, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '', 0), -- Hurricane R1
(41198, 0, 41198, 1, 0, 0, 0, 0, 0, -1, -1, 0, NULL, 0, 0, 0, 0, 0, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '', 0), -- Hurricane R2
(41170, 0, 41170, 1, 0, 0, 0, 0, 0, -1, -1, 0, NULL, 0, 0, 0, 0, 0, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '', 0); -- Hurricane R3

DELETE FROM `spell_script_names` WHERE `spell_id` IN (16914, 17401, 17402) AND `ScriptName` = 'classic_spell_ground_area_damage';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(16914, 'classic_spell_ground_area_damage'),
(17401, 'classic_spell_ground_area_damage'),
(17402, 'classic_spell_ground_area_damage');
