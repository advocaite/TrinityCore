-- Bitter Rivals (310, Kharanos): Guarded Thunderbrew Barrel (403) and Distracting Jarven (308) are meant to be offered while
-- Bitter Rivals is in the log, so both had PrevQuestID -310 (AzerothCore's rows; 2026_11_12_12 gave it to 308). But Bitter
-- Rivals starts with the Barleybrew Scalder (2548) in the bags and asks for that same keg, so it is complete the moment it is
-- taken. TrinityCore (Player::SatisfyQuestPreviousQuest) only takes a negative PrevQuestID while that quest is incomplete;
-- AzerothCore takes any status in the log. So for a player with the keg, neither the barrel nor Jarven offered anything
-- (self-test r10: ".playerbot selftest quest <bot> 310 add" then "... 308" -> refused by previous quest).
-- Now: no PrevQuestID, and a condition offers both while 310 is in the log, complete or not (QUESTSTATE mask 2|8).
-- 308 stays repeatable (2026_11_12_12). Safe to run again.
UPDATE `quest_template_addon` SET `PrevQuestID`=0 WHERE `ID` IN (308,403) AND `PrevQuestID`=-310;

DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=19 AND `SourceGroup`=0 AND `SourceEntry` IN (308,403) AND `ConditionTypeOrReference`=47 AND `ConditionValue1`=310;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
(19,0,308,0,0,47,0,310,10,0,'',0,0,0,'','Distracting Jarven - offered while Bitter Rivals is in the log (complete or incomplete)'),
(19,0,403,0,0,47,0,310,10,0,'',0,0,0,'','Guarded Thunderbrew Barrel - offered while Bitter Rivals is in the log (complete or incomplete)');
