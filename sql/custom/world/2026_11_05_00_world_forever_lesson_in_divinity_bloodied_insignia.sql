-- A Lesson in Divinity (94440): the Scarlet Crusade Attack Plans (264693) dropped from nothing, so the quest could not be done.
-- They drop again from creature 259433 (48%, only while on the quest), the source the AnoCore realm uses (Wowhead Forever kill
-- data). Nothing happens if creature 259433 is missing from the world DB.
UPDATE `creature_template_difficulty` SET `LootID` = `Entry` WHERE `Entry` = 259433 AND `DifficultyID` = 0 AND `LootID` = 0;
DELETE FROM `creature_loot_template` WHERE `Item` = 264693;
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`)
SELECT `LootID`, 0, 264693, 48, 1, 1, 0, 1, 1, 'Scarlet Crusade Attack Plans (A Lesson in Divinity)'
FROM `creature_template_difficulty` WHERE `Entry` = 259433 AND `DifficultyID` = 0 AND `LootID` > 0;

-- Bloodied Insignia (268535) starts an Alliance quest (95195), so it drops only for Alliance characters, as it does on the
-- live realm. Every creature loot row of the item gets the team condition (469 = Alliance). Safe to run again.
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 1 AND `SourceEntry` = 268535 AND `ConditionTypeOrReference` = 6;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`, `SourceGroup`, `SourceEntry`, `SourceId`, `ElseGroup`, `ConditionTypeOrReference`, `ConditionTarget`, `ConditionValue1`, `ConditionValue2`, `ConditionValue3`, `ConditionStringValue1`, `NegativeCondition`, `ErrorType`, `ErrorTextId`, `ScriptName`, `Comment`)
SELECT DISTINCT 1, `Entry`, 268535, 0, 0, 6, 0, 469, 0, 0, '', 0, 0, 0, '', 'Bloodied Insignia - Alliance only'
FROM `creature_loot_template` WHERE `Item` = 268535;
