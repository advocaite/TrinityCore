-- Classic 1.60: Taming Rod on a Windsong Crawler (spell 1280003, Skyborne hunter quest Taming the Beast 94978) always failed with
-- "Invalid target" and nothing reached the server. Since client 70532 the client's SpellMisc row 827917 has
-- SPELL_ATTR2_SPECIAL_TAMING_FLAG (0x400), the flag of Tame Beast 1515, so the client runs its own Tame Beast checks on the rod
-- and refuses the cast. The two other rods of the same quest line (1280046 Ornery Galestrider, 1271103 Vuldren Alpha) are the
-- same spell without the flag, and 70205 had no flag on 1280003 either. The server never reads the flag. Send the row back without
-- it (all other fields as in the client). Safe to run again.
DELETE FROM `spell_misc` WHERE `ID` = 827917;
INSERT INTO `spell_misc` (`ID`,`Attributes1`,`Attributes2`,`Attributes3`,`Attributes4`,`Attributes5`,`Attributes6`,`Attributes7`,`Attributes8`,`Attributes9`,`Attributes10`,`Attributes11`,`Attributes12`,`Attributes13`,`Attributes14`,`Attributes15`,`Attributes16`,`Attributes17`,`DifficultyID`,`CastingTimeIndex`,`DurationIndex`,`PvPDurationIndex`,`RangeIndex`,`SchoolMask`,`Speed`,`LaunchDelay`,`MinDuration`,`SpellIconFileDataID`,`ActiveIconFileDataID`,`ContentTuningID`,`ShowFutureSpellPlayerConditionID`,`SpellVisualScript`,`ActiveSpellVisualScript`,`SpellID`,`VerifiedBuild`) VALUES
(827917,65536,4,67174400,0,0,0,0,0,0,0,0,0,0,0,0,8192,0,0,1,18,0,4,8,0,0,0,132164,0,0,0,0,0,1280003,70685);

DELETE FROM `hotfix_data` WHERE `TableHash` = 0xC603EE28 AND `RecordId` = 827917;
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(984200,1281443100,0xC603EE28,827917,1,70685);
