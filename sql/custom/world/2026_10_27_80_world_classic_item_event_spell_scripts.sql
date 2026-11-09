-- Classic 1.60 (WoW Forever): vanilla item, quest and holiday spells whose retail scripts never attached (Validate failed or the
-- handler did not match the 1.60 spell data, 948952c1 / 260 start logs), so they did nothing. Classic scripts follow VMaNGOS
-- (C++: Custom/Classic/world/classic_spell_scripts.cpp). Consecration also gets the area data of its ground effect, so the
-- consecrated ground shows and the cast no longer logs "Invalid areatrigger create properties id". Safe to run again.

-- Savory Deviate Delight, Gnomish Mind Control Cap, Gadgetzan transporter backfire, Mistletoe, Reindeer Transformation,
-- Uther's / Grom's Tribute, Devour Magic (felhunter heal).
DELETE FROM `spell_script_names` WHERE
    (`spell_id` = 8213 AND `ScriptName` = 'spell_item_savory_deviate_delight') OR
    (`spell_id` = 13180 AND `ScriptName` = 'spell_item_mind_control_cap') OR
    (`spell_id` = 23448 AND `ScriptName` = 'spell_gen_gadgetzan_transporter_backfire') OR
    (`spell_id` = 26218 AND `ScriptName` = 'spell_winter_veil_mistletoe') OR
    (`spell_id` = 25860 AND `ScriptName` = 'spell_winter_veil_reindeer_transformation') OR
    (`spell_id` IN (24194, 24195) AND `ScriptName` = 'spell_quest_uther_grom_tribute') OR
    (`spell_id` = 19505 AND `ScriptName` = 'spell_warl_devour_magic');
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('classic_spell_item_savory_deviate_delight', 'classic_spell_item_mind_control_cap',
    'classic_spell_gen_gadgetzan_transporter_backfire', 'classic_spell_winter_veil_mistletoe', 'classic_spell_winter_veil_reindeer_transformation',
    'classic_spell_quest_uther_grom_tribute', 'classic_spell_warl_devour_magic');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(8213, 'classic_spell_item_savory_deviate_delight'),
(13180, 'classic_spell_item_mind_control_cap'),
(23448, 'classic_spell_gen_gadgetzan_transporter_backfire'),
(26218, 'classic_spell_winter_veil_mistletoe'),
(25860, 'classic_spell_winter_veil_reindeer_transformation'),
(24194, 'classic_spell_quest_uther_grom_tribute'),
(24195, 'classic_spell_quest_uther_grom_tribute'),
(19505, 'classic_spell_warl_devour_magic'),
(19731, 'classic_spell_warl_devour_magic'),
(19734, 'classic_spell_warl_devour_magic'),
(19736, 'classic_spell_warl_devour_magic');

-- Consecration ranks 1-5 (26573, 20116, 20922, 20923, 20924): effect 1 creates area trigger 41311 / 41313 / 41314 / 41315 / 41316,
-- 8 yd sphere as the client radius (index 14), like Blizzard. The damage ticks stay classic_spell_pal_consecration.
DELETE FROM `areatrigger_template` WHERE `Id` IN (41311,41313,41314,41315,41316) AND `IsCustom` = 1;
INSERT INTO `areatrigger_template` (`Id`, `IsCustom`, `Flags`, `ActionSetId`, `ActionSetFlags`, `VerifiedBuild`) VALUES
(41311, 1, 0, 0, 0, 0),
(41313, 1, 0, 0, 0, 0),
(41314, 1, 0, 0, 0, 0),
(41315, 1, 0, 0, 0, 0),
(41316, 1, 0, 0, 0, 0);

DELETE FROM `areatrigger_create_properties` WHERE `Id` IN (41311,41313,41314,41315,41316) AND `IsCustom` = 0;
INSERT INTO `areatrigger_create_properties` (`Id`, `IsCustom`, `AreaTriggerId`, `IsAreatriggerCustom`, `Flags`, `MoveCurveId`,
`ScaleCurveId`, `MorphCurveId`, `FacingCurveId`, `AnimId`, `AnimKitId`, `DecalPropertiesId`, `SpellForVisuals`, `PositionalSoundKitId`,
`TimeToTargetScale`, `Speed`, `SpeedIsTime`, `Shape`, `ShapeData0`, `ShapeData1`, `ShapeData2`, `ShapeData3`, `ShapeData4`,
`ShapeData5`, `ShapeData6`, `ShapeData7`, `Roll`, `Pitch`, `Yaw`, `TargetRoll`, `TargetPitch`, `TargetYaw`, `ScriptName`,
`VerifiedBuild`) VALUES
(41311, 0, 41311, 1, 0, 0, 0, 0, 0, -1, -1, 0, NULL, 0, 0, 0, 0, 0, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '', 0), -- Consecration R1
(41313, 0, 41313, 1, 0, 0, 0, 0, 0, -1, -1, 0, NULL, 0, 0, 0, 0, 0, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '', 0), -- Consecration R2
(41314, 0, 41314, 1, 0, 0, 0, 0, 0, -1, -1, 0, NULL, 0, 0, 0, 0, 0, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '', 0), -- Consecration R3
(41315, 0, 41315, 1, 0, 0, 0, 0, 0, -1, -1, 0, NULL, 0, 0, 0, 0, 0, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '', 0), -- Consecration R4
(41316, 0, 41316, 1, 0, 0, 0, 0, 0, -1, -1, 0, NULL, 0, 0, 0, 0, 0, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '', 0); -- Consecration R5
