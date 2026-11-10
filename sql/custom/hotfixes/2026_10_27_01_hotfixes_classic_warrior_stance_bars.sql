-- Classic 1.60 (WoW Forever): warrior stance action bars. The 70532 SpellShapeshiftForm.db2 has BonusActionBar 0 for Battle Stance (17),
-- Defensive Stance (18) and Berserker Stance (19), so the client never pages the main action bar on a stance change. Druid forms page
-- because Cat Form (1) has bar 1 and Bear/Dire Bear Form (5, 8) bar 3. The server sends the stance form the same way as a druid form
-- (aura 36 -> UnitData ShapeshiftForm); the bar page is picked by the client from this table. These rows are the 70532 rows unchanged
-- except for the vanilla bars (Battle 1, Defensive 2, Berserker 3), sent to clients through hotfix_data. To drop the change, delete
-- this file and run the two DELETE lines. Check in game: put a different ability on the main bar in each stance; switching stances
-- shows each stance's own bar. Stealth (30) has bar 0 in 70532 too; vanilla rogues paged to bar 1 in stealth, so its row gets bar 1
-- the same way (70532 row, flags 1). Check: a different ability on slot 1 in and out of Stealth. Safe to run again.
DELETE FROM `spell_shapeshift_form` WHERE `ID` IN (17,18,19,30);
INSERT INTO `spell_shapeshift_form` (`ID`,`Name`,`CreatureDisplayID`,`CreatureType`,`Flags`,`AttackIconFileID`,`BonusActionBar`,`CombatRoundTime`,`DamageVariance`,`MountTypeID`,`PresetSpellID1`,`PresetSpellID2`,`PresetSpellID3`,`PresetSpellID4`,`PresetSpellID5`,`PresetSpellID6`,`PresetSpellID7`,`PresetSpellID8`,`VerifiedBuild`) VALUES
(17,'Battle Stance',0,0,7,0,1,0,0,0,0,0,0,0,0,0,0,0,70532),
(18,'Defensive Stance',0,0,7,0,2,0,0,0,0,0,0,0,0,0,0,0,70532),
(19,'Berserker Stance',0,0,7,0,3,0,0,0,0,0,0,0,0,0,0,0,70532),
(30,'Stealth',0,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,70532);

DELETE FROM `hotfix_data` WHERE `TableHash` = 4182948165 AND `RecordId` IN (17,18,19,30);
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(2000201,1397720201,4182948165,17,1,70532),
(2000202,1397720202,4182948165,18,1,70532),
(2000203,1397720203,4182948165,19,1,70532),
(2000204,1397720204,4182948165,30,1,70532);
