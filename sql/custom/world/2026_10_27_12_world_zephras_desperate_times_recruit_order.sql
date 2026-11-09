-- Desperate Times (92640): "Recruit the Windshapers" 465809 and "Recruit the High Order" 465810 are sequenced objectives
-- (QUEST_OBJECTIVE_FLAG_SEQUENCED) after "Speak with Valennia Stormfist" 465840, and the core gives them no credit before it.
-- Ayessa's and Elaadrin's recruit options showed from the start anyway; they now wait for Valennia too.
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 15 AND `SourceGroup` IN (40962, 40964) AND `SourceEntry` = 0 AND `ConditionTypeOrReference` = 48 AND `ConditionValue1` = 465840;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`, `SourceGroup`, `SourceEntry`, `SourceId`, `ElseGroup`, `ConditionTypeOrReference`, `ConditionTarget`, `ConditionValue1`, `ConditionValue2`, `ConditionValue3`, `NegativeCondition`, `ErrorType`, `ErrorTextId`, `ScriptName`, `Comment`) VALUES
(15, 40962, 0, 0, 0, 48, 0, 465840, 0, 1, 0, 0, 0, '', 'Recruit the Windshapers - after Speak with Valennia Stormfist (sequenced objective)'),
(15, 40964, 0, 0, 0, 48, 0, 465840, 0, 1, 0, 0, 0, '', 'Recruit the High Order - after Speak with Valennia Stormfist (sequenced objective)');
