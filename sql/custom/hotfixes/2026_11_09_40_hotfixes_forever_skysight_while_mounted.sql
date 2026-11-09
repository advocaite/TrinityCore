-- Skyborne racial Skysight 1259686: its buff Elemental Blessing 1259688 gives +15% speed and +15% mounted speed, but Skysight itself
-- could not be cast while mounted (no SPELL_ATTR0_ALLOW_WHILE_MOUNTED 0x01000000), so the mounted part never helped (Discord 2026-11-09).
-- Same row as the client's SpellMisc 813206 with that attribute added; the server sets it too (SpellMgr::LoadSpellInfoCorrections).
-- Own hotfix_data id 2059100. Safe to run again.
DELETE FROM `spell_misc` WHERE `ID` = 813206;
INSERT INTO `spell_misc` (`ID`,`Attributes1`,`Attributes2`,`Attributes3`,`Attributes4`,`Attributes5`,`Attributes6`,`Attributes7`,`Attributes8`,`Attributes9`,`Attributes10`,`Attributes11`,`Attributes12`,`Attributes13`,`Attributes14`,`Attributes15`,`Attributes16`,`Attributes17`,`DifficultyID`,`CastingTimeIndex`,`DurationIndex`,`PvPDurationIndex`,`RangeIndex`,`SchoolMask`,`Speed`,`LaunchDelay`,`MinDuration`,`SpellIconFileDataID`,`ActiveIconFileDataID`,`ContentTuningID`,`ShowFutureSpellPlayerConditionID`,`SpellVisualScript`,`ActiveSpellVisualScript`,`SpellID`,`VerifiedBuild`) VALUES
(813206,16777216,0,0,0,65536,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,1,8,0.0,0.0,0.0,1029587,0,0,0,0,0,1259686,70862);
DELETE FROM `hotfix_data` WHERE `Id` = 2059100;
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(2059100,1397779100,3322146344,813206,1,70862);
