-- Cozy Sleeping Bag (211527): "Unfurl a sleeping bag. Resting inside for at least one minute will provide a bonus to experience earned,
-- stacking up to 3 times." Using it did nothing that a player could see or get (Discord, shredxgod and two more 2026-11-10: the bag
-- does not unfold). Its spell Unroll Bag 429961 makes area trigger 30821 (effect 0) and the Cozy Sleeping Bag creature 214589 (effect 1),
-- both 3 yards in front of the player for 20 minutes. Area trigger 30821 had no server data, so no area trigger was made and the bonus
-- never came: Well-Rested Tracking Aura 429960 (every 60 s: Well-Rested 429959, +1% experience, up to 3 stacks) was never put on.
-- Now: a 3 yard sphere (the radius of effect 1) that puts 429960 on the bag's owner while they are inside it; leaving it takes the
-- tracking aura off again (Well-Rested itself stays for its own duration).
-- INFERRED: the shape and size (no sniff of the bag), and that only the owner gets it (a personal item).
-- Safe to run again.
DELETE FROM `areatrigger_template` WHERE `Id` = 30821 AND `IsCustom` = 1;
INSERT INTO `areatrigger_template` (`Id`, `IsCustom`, `Flags`, `ActionSetId`, `ActionSetFlags`, `VerifiedBuild`) VALUES
(30821, 1, 0, 0, 0, 0);
DELETE FROM `areatrigger_template_actions` WHERE `AreaTriggerId` = 30821 AND `IsCustom` = 1;
INSERT INTO `areatrigger_template_actions` (`AreaTriggerId`, `IsCustom`, `ActionType`, `ActionParam`, `TargetType`) VALUES
(30821, 1, 1, 429960, 5);
DELETE FROM `areatrigger_create_properties` WHERE `Id` = 30821 AND `IsCustom` = 0;
INSERT INTO `areatrigger_create_properties` (`Id`, `IsCustom`, `AreaTriggerId`, `IsAreatriggerCustom`, `Flags`, `MoveCurveId`,
`ScaleCurveId`, `MorphCurveId`, `FacingCurveId`, `AnimId`, `AnimKitId`, `DecalPropertiesId`, `SpellForVisuals`, `PositionalSoundKitId`,
`TimeToTargetScale`, `Speed`, `SpeedIsTime`, `Shape`, `ShapeData0`, `ShapeData1`, `ShapeData2`, `ShapeData3`, `ShapeData4`,
`ShapeData5`, `ShapeData6`, `ShapeData7`, `Roll`, `Pitch`, `Yaw`, `TargetRoll`, `TargetPitch`, `TargetYaw`, `ScriptName`,
`VerifiedBuild`) VALUES
(30821, 0, 30821, 1, 0, 0, 0, 0, 0, -1, -1, 0, NULL, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '', 0);
