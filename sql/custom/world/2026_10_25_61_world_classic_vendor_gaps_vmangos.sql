-- Classic 1.60: two vanilla vendors sell less than in VMaNGOS 1.12 (every spawned vendor's list was compared with VMaNGOS, counting only
-- items the 70532 client has). Professor Thaddeus Paleo (Darkmoon Faire) lacked his four vials, Evie Whirlbrew (Everlook) the
-- tailoring pattern 14526 that Qia next to her also sells. Unlimited stock as in VMaNGOS. Safe to run again.

DELETE FROM `npc_vendor` WHERE (`entry` = 14847 AND `item` IN (3371,3372,8925,18256)) OR (`entry` = 11188 AND `item` = 14526);
INSERT INTO `npc_vendor` (`entry`,`slot`,`item`,`maxcount`,`incrtime`,`ExtendedCost`,`type`,`BonusListIDs`,`PlayerConditionID`,`IgnoreFiltering`,`VerifiedBuild`) VALUES
(14847,0,3371,0,0,0,1,NULL,0,0,0),
(14847,0,3372,0,0,0,1,NULL,0,0,0),
(14847,0,8925,0,0,0,1,NULL,0,0,0),
(14847,0,18256,0,0,0,1,NULL,0,0,0),
(11188,340,14526,0,0,0,1,NULL,0,0,0);
