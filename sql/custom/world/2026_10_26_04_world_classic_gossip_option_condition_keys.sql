-- Gossip option conditions (SourceType 15) on seven Zephras menus used the option's GossipOptionID as SourceEntry, but
-- ConditionMgr::addToGossipMenuItems matches SourceEntry against gossip_menu_option.OptionID (the option index). The
-- conditions were rejected at startup ("GossipMenuId X Item Y not found") and the options showed for everyone, e.g. the
-- Desperate Times (92640) recruit options before the quest. Menus 40780, 40962, 40964, 41313, 41553, 41923, 42169.
UPDATE `conditions` c
JOIN `gossip_menu_option` g ON g.`MenuID` = c.`SourceGroup` AND g.`GossipOptionID` = c.`SourceEntry`
SET c.`SourceEntry` = g.`OptionID`
WHERE c.`SourceTypeOrReferenceId` = 15 AND g.`OptionID` <> c.`SourceEntry`;

-- Desperate Times: the recruit objectives (465809, 465810) are sequenced after "Speak with Valennia Stormfist" (465840),
-- so Ayessa's and Elaadrin's recruit options wait for it too.
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 15 AND `SourceGroup` IN (40962, 40964) AND `SourceEntry` = 0 AND `ConditionTypeOrReference` = 48 AND `ConditionValue1` = 465840;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`, `SourceGroup`, `SourceEntry`, `SourceId`, `ElseGroup`, `ConditionTypeOrReference`, `ConditionTarget`, `ConditionValue1`, `ConditionValue2`, `ConditionValue3`, `ConditionStringValue1`, `NegativeCondition`, `ErrorType`, `ErrorTextId`, `ScriptName`, `Comment`) VALUES
(15, 40962, 0, 0, 0, 48, 0, 465840, 0, 1, '', 0, 0, 0, '', 'Ayessa Dawnsinger - show option only after speaking with Valennia (Desperate Times)'),
(15, 40964, 0, 0, 0, 48, 0, 465840, 0, 1, '', 0, 0, 0, '', 'Elaadrin Evengale - show option only after speaking with Valennia (Desperate Times)');
