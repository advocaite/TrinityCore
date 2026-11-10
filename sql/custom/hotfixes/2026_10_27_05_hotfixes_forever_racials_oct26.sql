-- Racial abilities and traits from the WoW Forever beta development notes of 2026-10-26 (Blizzard forum thread 2360696, post 7).
-- Client 70630 still carries the old values and has no new spells, so the server sends the changes as hotfixes: the same records
-- (same IDs) with the new values, which the server also loads over its DB2 data, so tooltips and effects agree.
--   Human:    Ruthless 20597 2% -> 1% crit; The Human Spirit 20598 5% -> 10% Spirit (its vanilla value row goes in 2026_10_27_96 world).
--   Dwarf:    Earth Fury 1259719 (2% crit damage and healing) becomes Artisan's Precision: 1% melee / ranged hit (aura 54) and 1% spell hit (aura 55).
--   Tauren:   Plainsrunning 1259918 2% per step up to 60% (speed aura 1299038 +2% per stack, still 30 stacks), no mounted bonus;
--             Endurance 20550 keeps only its 5% Health (critical damage / healing effects at 0).
--   Orc:      Savage 20574 (1% crit) becomes Command: a dummy aura whose pet aura (spell_pet_auras, world 2026_10_27_96) gives
--             Hunter and Warlock pets 5% damage done.
--   Skyborne: Wind Blessed 1259710 3% -> 2% haste. Read Leyline: Energized 1270842 50% regeneration for 15 sec (was 100%, 30 sec); the
--             former 15 min Energized 1259691 becomes Attuned (1 hour, kept through death, doubles the next buffs). Skysight: Elemental
--             Blessing 1259688 15% speed for 15 sec (was 10%, 30 sec); the former 15 min one 1270893 becomes Charged (1 hour, kept
--             through death, raises it to 30%). The doubling and the Innervate rule are in classic_spell_skyborne_racial_focus.
-- Spell.db2 texts go through hotfix_blob (enUS only: other locales keep their client text). Own hotfix_data ids 2000300-2000399.
-- Icons stay the client's (no Forever icon for Artisan's Precision or the new Command yet). When a client build ships these
-- changes with its own records, the client diff lists these rows under hotfix_other_differ: drop them then. Safe to run again.
DELETE FROM `spell_effect` WHERE `ID` IN (697219,698126,1268914,1363668,1322984,1349372,697940,1268898,1354094,1269249,1322990,1363462,1323003,1283214,1283215,1268888,1268889,1268883,1268884,1283299,1283300);
INSERT INTO `spell_effect` (`ID`,`EffectAura`,`DifficultyID`,`EffectIndex`,`Effect`,`EffectAmplitude`,`EffectAttributes`,`EffectAuraPeriod`,`EffectBonusCoefficient`,`EffectChainAmplitude`,`EffectChainTargets`,`EffectItemType`,`EffectMechanic`,`EffectPointsPerResource`,`EffectPosFacing`,`EffectRealPointsPerLevel`,`EffectTriggerSpell`,`BonusCoefficientFromAP`,`PvpMultiplier`,`Coefficient`,`Variance`,`ResourceCoefficient`,`GroupSizeBasePointsCoefficient`,`EffectBasePoints`,`ScalingClass`,`TargetNodeGraph`,`EffectMiscValue1`,`EffectMiscValue2`,`EffectRadiusIndex1`,`EffectRadiusIndex2`,`EffectSpellClassMask1`,`EffectSpellClassMask2`,`EffectSpellClassMask3`,`EffectSpellClassMask4`,`ImplicitTarget1`,`ImplicitTarget2`,`SpellID`,`VerifiedBuild`) VALUES
(697219,290,0,0,6,0.0,0,0,1.0,1.0,0,0,0,0.0,0.0,0.0,0,0.0,1.0,0.0,0.0,0.0,1.0,1.0,0,0,0,0,0,0,0,0,0,0,1,0,20597,70630),
(698126,137,0,0,6,0.0,0,0,1.0,1.0,0,0,0,0.0,0.0,0.0,0,0.0,1.0,0.0,0.0,0.0,1.0,10.0,0,0,4,16,0,0,0,0,0,0,1,0,20598,70630),
(1268914,54,0,0,6,0.0,0,0,1.0,1.0,0,0,0,0.0,0.0,0.0,0,0.0,1.0,0.0,0.0,0.0,1.0,1.0,0,0,0,0,0,0,0,0,0,0,1,0,1259719,70630),
(1363668,55,0,1,6,0.0,0,0,1.0,1.0,0,0,0,0.0,0.0,0.0,0,0.0,1.0,0.0,0.0,0.0,1.0,1.0,0,0,0,0,0,0,0,0,0,0,1,0,1259719,70630),
(1322984,163,0,1,6,0.0,0,0,1.0,1.0,0,0,0,0.0,0.0,0.0,0,0.0,1.0,0.0,0.0,0.0,1.0,0.0,0,0,0,0,0,0,0,0,0,0,1,0,20550,70630),
(1349372,50,0,2,6,0.0,0,0,1.0,1.0,0,0,0,0.0,0.0,0.0,0,0.0,1.0,0.0,0.0,0.0,1.0,0.0,0,0,0,0,0,0,0,0,0,0,1,0,20550,70630),
(697940,4,0,0,6,0.0,0,0,1.0,1.0,0,0,0,0.0,0.0,0.0,0,0.0,1.0,0.0,0.0,0.0,1.0,5.0,0,0,0,0,0,0,0,0,0,0,1,0,20574,70630),
(1268898,342,0,0,6,0.0,0,0,1.0,1.0,0,0,0,0.0,0.0,0.0,0,0.0,1.0,0.0,0.0,0.0,1.0,2.0,0,0,0,0,0,0,0,0,0,0,1,0,1259710,70630),
(1354094,65,0,1,6,0.0,0,0,1.0,1.0,0,0,0,0.0,0.0,0.0,0,0.0,1.0,0.0,0.0,0.0,1.0,2.0,0,0,0,0,0,0,0,0,0,0,1,0,1259710,70630),
(1269249,226,0,0,6,0.0,0,1000,0.0,1.0,0,0,0,0.0,0.0,0.0,0,0.0,1.0,0.0,0.0,0.0,1.0,2.0,0,0,0,0,0,0,0,0,0,0,1,0,1259918,70630),
(1322990,4,0,2,6,0.0,0,0,0.0,1.0,0,0,0,0.0,0.0,0.0,0,0.0,1.0,0.0,0.0,0.0,1.0,60.0,0,0,0,0,0,0,0,0,0,0,1,0,1259918,70630),
(1363462,130,0,3,6,0.0,0,0,0.0,1.0,0,0,0,0.0,0.0,0.0,0,0.0,1.0,0.0,0.0,0.0,1.0,0.0,0,0,0,0,0,0,0,0,0,0,1,0,1259918,70630),
(1323003,31,0,0,6,0.0,0,0,0.0,1.0,0,0,0,0.0,0.0,0.0,0,0.0,1.0,0.0,0.0,0.0,1.0,2.0,0,0,0,0,0,0,0,0,0,0,1,0,1299038,70630),
(1283214,110,0,0,6,0.0,0,0,1.0,1.0,0,0,0,0.0,0.0,0.0,0,0.0,1.0,0.0,0.0,0.0,1.0,50.0,0,0,0,0,0,0,0,0,0,0,1,0,1270842,70630),
(1283215,88,0,1,6,0.0,0,0,1.0,1.0,0,0,0,0.0,0.0,0.0,0,0.0,1.0,0.0,0.0,0.0,1.0,50.0,0,0,0,0,0,0,0,0,0,0,1,0,1270842,70630),
(1268888,4,0,0,6,0.0,0,0,1.0,1.0,0,0,0,0.0,0.0,0.0,0,0.0,1.0,0.0,0.0,0.0,1.0,0.0,0,0,0,0,0,0,0,0,0,0,1,0,1259691,70630),
(1268889,4,0,1,6,0.0,0,0,1.0,1.0,0,0,0,0.0,0.0,0.0,0,0.0,1.0,0.0,0.0,0.0,1.0,0.0,0,0,0,0,0,0,0,0,0,0,1,0,1259691,70630),
(1268883,31,0,0,6,0.0,0,0,1.0,1.0,0,0,0,0.0,0.0,0.0,0,0.0,1.0,0.0,0.0,0.0,1.0,15.0,0,0,0,0,0,0,0,0,0,0,1,0,1259688,70630),
(1268884,172,0,1,6,0.0,0,0,1.0,1.0,0,0,0,0.0,0.0,0.0,0,0.0,1.0,0.0,0.0,0.0,1.0,15.0,0,0,0,0,0,0,0,0,0,0,1,0,1259688,70630),
(1283299,4,0,0,6,0.0,0,0,1.0,1.0,0,0,0,0.0,0.0,0.0,0,0.0,1.0,0.0,0.0,0.0,1.0,0.0,0,0,0,0,0,0,0,0,0,0,1,0,1270893,70630),
(1283300,4,0,1,6,0.0,0,0,1.0,1.0,0,0,0,0.0,0.0,0.0,0,0.0,1.0,0.0,0.0,0.0,1.0,0.0,0,0,0,0,0,0,0,0,0,0,1,0,1270893,70630);

DELETE FROM `spell_misc` WHERE `ID` IN (821912,813208,813209,821963);
INSERT INTO `spell_misc` (`ID`,`Attributes1`,`Attributes2`,`Attributes3`,`Attributes4`,`Attributes5`,`Attributes6`,`Attributes7`,`Attributes8`,`Attributes9`,`Attributes10`,`Attributes11`,`Attributes12`,`Attributes13`,`Attributes14`,`Attributes15`,`Attributes16`,`Attributes17`,`DifficultyID`,`CastingTimeIndex`,`DurationIndex`,`PvPDurationIndex`,`RangeIndex`,`SchoolMask`,`Speed`,`LaunchDelay`,`MinDuration`,`SpellIconFileDataID`,`ActiveIconFileDataID`,`ContentTuningID`,`ShowFutureSpellPlayerConditionID`,`SpellVisualScript`,`ActiveSpellVisualScript`,`SpellID`,`VerifiedBuild`) VALUES
(821912,0,0,4,0,1048576,0,0,0,0,0,0,0,0,0,0,8192,0,0,1,8,0,1,64,0.0,0.0,0.0,135729,0,0,0,0,0,1270842,70630),
(813208,0,0,4,0,1048576,0,0,0,0,0,0,0,0,0,0,8192,0,0,1,8,0,1,8,0.0,0.0,0.0,1381298,0,0,0,0,0,1259688,70630),
(813209,0,0,132,1048576,0,0,0,0,0,0,0,0,0,0,0,8192,0,0,1,42,0,1,64,0.0,0.0,0.0,135729,0,0,0,0,0,1259691,70630),
(821963,0,0,132,1048576,0,0,0,0,0,0,0,0,0,0,0,8192,0,0,1,42,0,1,8,0.0,0.0,0.0,1381298,0,0,0,0,0,1270893,70630);

DELETE FROM `spell_name` WHERE `ID` IN (1259719,20574,1259691,1270893);
INSERT INTO `spell_name` (`ID`,`Name`,`VerifiedBuild`) VALUES
(1259719,'Artisan''s Precision',70630),
(20574,'Command',70630),
(1259691,'Attuned',70630),
(1270893,'Charged',70630);

-- Spell.db2 (no server store): blob = NameSubtext, NUL, Description, NUL, AuraDescription, NUL
DELETE FROM `hotfix_blob` WHERE `TableHash` = 3776013982 AND `RecordId` IN (1259719,20550,20574,1259918,1259705,1259686,1259691,1270893);
INSERT INTO `hotfix_blob` (`TableHash`,`RecordId`,`locale`,`Blob`,`VerifiedBuild`) VALUES
(3776013982,1259719,'enUS',CONCAT('Racial Passive',CHAR(0),'Increases your chance to hit with spells and attacks by $s1%.',CHAR(0),'',CHAR(0)),70630),
(3776013982,20550,'enUS',CONCAT('Racial Passive',CHAR(0),'Total Health increased by $s1%.',CHAR(0),'',CHAR(0)),70630),
(3776013982,20574,'enUS',CONCAT('Racial Passive',CHAR(0),'Damage dealt by Hunter and Warlock pets increased by $s1%.',CHAR(0),'',CHAR(0)),70630),
(3776013982,1259918,'enUS',CONCAT('Racial Passive',CHAR(0),'Gain $s1% increased movement speed every $s2 sec spent moving, up to a maximum of $s3% increase. Taking damage or standing still will reduce this effect.',CHAR(0),'',CHAR(0)),70630),
(3776013982,1259705,'enUS',CONCAT('Racial',CHAR(0),'Attempt to tap into the power of a nearby leyline, increasing your Health and Mana regeneration by $1270842s1% for $1270842d. Using it at a leyline also grants Attuned for $1259691d, which raises the bonus to 100%. Cannot be used while Innervate is active.',CHAR(0),'',CHAR(0)),70630),
(3776013982,1259686,'enUS',CONCAT('Racial',CHAR(0),'Attempt to draw power from a convergence of elements and receive its blessing, increasing your movement and mounted movement speeds by $1259688s1% for $1259688d. Using it at an elemental convergence also grants Charged for $1270893d, which raises the bonus to 30%.',CHAR(0),'',CHAR(0)),70630),
(3776013982,1259691,'enUS',CONCAT('',CHAR(0),'',CHAR(0),'Read Leyline increases Health and Mana regeneration by 100%. Persists through death.',CHAR(0)),70630),
(3776013982,1270893,'enUS',CONCAT('',CHAR(0),'',CHAR(0),'Skysight increases movement speed by 30%. Persists through death.',CHAR(0)),70630);

DELETE FROM `hotfix_data` WHERE `Id` BETWEEN 2000300 AND 2000399;
INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES
(2000300,1397720300,4030871717,697219,1,70630),
(2000301,1397720301,4030871717,698126,1,70630),
(2000302,1397720302,4030871717,1268914,1,70630),
(2000303,1397720303,4030871717,1363668,1,70630),
(2000304,1397720304,4030871717,1322984,1,70630),
(2000305,1397720305,4030871717,1349372,1,70630),
(2000306,1397720306,4030871717,697940,1,70630),
(2000307,1397720307,4030871717,1268898,1,70630),
(2000308,1397720308,4030871717,1354094,1,70630),
(2000309,1397720309,4030871717,1269249,1,70630),
(2000310,1397720310,4030871717,1322990,1,70630),
(2000311,1397720311,4030871717,1363462,1,70630),
(2000312,1397720312,4030871717,1323003,1,70630),
(2000313,1397720313,4030871717,1283214,1,70630),
(2000314,1397720314,4030871717,1283215,1,70630),
(2000315,1397720315,4030871717,1268888,1,70630),
(2000316,1397720316,4030871717,1268889,1,70630),
(2000317,1397720317,4030871717,1268883,1,70630),
(2000318,1397720318,4030871717,1268884,1,70630),
(2000319,1397720319,4030871717,1283299,1,70630),
(2000320,1397720320,4030871717,1283300,1,70630),
(2000321,1397720321,3322146344,821912,1,70630),
(2000322,1397720322,3322146344,813208,1,70630),
(2000323,1397720323,3322146344,813209,1,70630),
(2000324,1397720324,3322146344,821963,1,70630),
(2000325,1397720325,1187407512,1259719,1,70630),
(2000326,1397720326,1187407512,20574,1,70630),
(2000327,1397720327,1187407512,1259691,1,70630),
(2000328,1397720328,1187407512,1270893,1,70630),
(2000329,1397720329,3776013982,1259719,1,70630),
(2000330,1397720330,3776013982,20550,1,70630),
(2000331,1397720331,3776013982,20574,1,70630),
(2000332,1397720332,3776013982,1259918,1,70630),
(2000333,1397720333,3776013982,1259705,1,70630),
(2000334,1397720334,3776013982,1259686,1,70630),
(2000335,1397720335,3776013982,1259691,1,70630),
(2000336,1397720336,3776013982,1270893,1,70630);
