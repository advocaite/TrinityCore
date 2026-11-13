-- Crest of Lordaeron (Ruins of Lordaeron, gameobject 614679) gave Alliance players the Horde quest (player report 2026-11-13,
-- Discord "bugs 2" item 3). There are two crests: item 268579 starts the Alliance quest 95189 (to Lady Dena Kennedy in
-- Stormwind) and item 275521 starts the Horde quest 95204 (to Oran Snakewrithe in Undercity), each by its own StartQuestID in
-- the client item data. The loot came from one sniff opening by a Horde character, so only the Horde crest was in it and every
-- player got that one. Now each faction finds its own crest (Questie Forever: 268579 drops from 614679).
-- Safe to run again.

DELETE FROM `gameobject_loot_template` WHERE `Entry` = 614679 AND `Item` IN (268579,275521);
INSERT INTO `gameobject_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(614679,0,268579,100.0,0,1,0,1,1,'Crest of Lordaeron (Alliance, starts 95189)'),
(614679,0,275521,100.0,0,1,0,1,1,'Crest of Lordaeron (Horde, starts 95204)');

DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 4 AND `SourceGroup` = 614679 AND `SourceEntry` IN (268579,275521);
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
(4,614679,268579,0,0,6,0,469,0,0,'',0,0,0,'','Crest of Lordaeron - Alliance crest only for Alliance'),
(4,614679,275521,0,0,6,0,67,0,0,'',0,0,0,'','Crest of Lordaeron - Horde crest only for Horde');
