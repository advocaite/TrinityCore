-- NPCs in bear form show as a black shape (Discord 2026-11-10, voldemargreed "bugs" item 2: Rageclaw 7318, the druid of the Druid of
-- the Claw quest, "broken bear form", screenshot: a black low-poly shape under his name plate).
-- Rageclaw casts Bear Form 7090 (SmartAI), shapeshift form 14 "Creature - Bear". The client's SpellShapeshiftForm 14 points at
-- creature display 147881 (model 18225 with texture file 123679, an old bear skin); the Classic 1.60 client draws it untextured.
-- The druid Bear Form (form 5) uses display 2281, the Classic bear the client draws properly. Form 14 now uses that display too
-- (INFERRED: the missing skin is the cause; the client has the model file). Form 14 is only used by creature spells.
-- hotfix_data ids 2112000-2112009 (bugs thread, 2026-11-12). Safe to run again.
DELETE FROM `spell_shapeshift_form` WHERE `ID` = 14;
INSERT INTO `spell_shapeshift_form` (`ID`,`Name`,`CreatureDisplayID`,`CreatureType`,`Flags`,`AttackIconFileID`,`BonusActionBar`,`CombatRoundTime`,
  `DamageVariance`,`MountTypeID`,`PresetSpellID1`,`PresetSpellID2`,`PresetSpellID3`,`PresetSpellID4`,`PresetSpellID5`,`PresetSpellID6`,
  `PresetSpellID7`,`PresetSpellID8`,`VerifiedBuild`) VALUES
(14,'Creature - Bear',2281,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70888);

DELETE FROM `hotfix_data` WHERE `Id` BETWEEN 2112000 AND 2112009;
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(2112000,1411120000,4182948165,14,1,70888);
