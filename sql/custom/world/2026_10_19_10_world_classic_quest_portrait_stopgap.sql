-- Stop-gap: the client throws a Lua error ("attempt to index local 'rider'", QuestFrame_ShowQuestPortrait) when a quest has a
-- portrait giver/turn-in but PortraitGiverModelSceneID is 0 (sent as nil, so the portrait scene has no actors). The quests imported
-- from the vanilla data (e.g. 218 "The Stolen Journal") have no model scene, so their portrait is cleared until the right
-- scene ID is known; quests that carry a model scene are left alone. Safe to run again.
UPDATE `quest_template` SET `PortraitGiver` = 0, `PortraitGiverMount` = 0, `PortraitGiverText` = '', `PortraitGiverName` = ''
WHERE `PortraitGiver` <> 0 AND `PortraitGiverModelSceneID` = 0;
UPDATE `quest_template` SET `PortraitTurnIn` = 0, `PortraitTurnInText` = '', `PortraitTurnInName` = ''
WHERE `PortraitTurnIn` <> 0 AND `PortraitGiverModelSceneID` = 0;
