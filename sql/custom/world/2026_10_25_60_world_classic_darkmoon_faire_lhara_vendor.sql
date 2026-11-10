-- Classic 1.60: Lhara 14846 at the Darkmoon Faire sold only Cataclysm pets and mounts (72140, 73762-73905, 74981, 91003), none of
-- which the 70532 client has, so her vendor window was empty. She gets her VMaNGOS 1.12 list: herbs, leathers, a few bows and
-- weapons, and the Darkmoon Ring and Necklace, with the same limited stock and restock times. Every item exists in 70532.
-- Safe to run again.

DELETE FROM `npc_vendor` WHERE `entry` = 14846;
INSERT INTO `npc_vendor` (`entry`,`slot`,`item`,`maxcount`,`incrtime`,`ExtendedCost`,`type`,`BonusListIDs`,`PlayerConditionID`,`IgnoreFiltering`,`VerifiedBuild`) VALUES
(14846,0,785,1,7200,0,1,NULL,0,0,0),
(14846,1,2319,5,7200,0,1,NULL,0,0,0),
(14846,2,2449,3,7200,0,1,NULL,0,0,0),
(14846,3,2453,1,7200,0,1,NULL,0,0,0),
(14846,4,3355,2,7200,0,1,NULL,0,0,0),
(14846,5,3356,4,7200,0,1,NULL,0,0,0),
(14846,6,3357,3,7200,0,1,NULL,0,0,0),
(14846,7,3358,4,7200,0,1,NULL,0,0,0),
(14846,8,4234,4,7200,0,1,NULL,0,0,0),
(14846,9,4289,0,0,0,1,NULL,0,0,0),
(14846,10,4304,3,7200,0,1,NULL,0,0,0),
(14846,11,8170,4,43200,0,1,NULL,0,0,0),
(14846,12,8845,1,43200,0,1,NULL,0,0,0),
(14846,13,11303,1,43200,0,1,NULL,0,0,0),
(14846,14,11305,1,43200,0,1,NULL,0,0,0),
(14846,15,11306,1,43200,0,1,NULL,0,0,0),
(14846,16,11308,1,43200,0,1,NULL,0,0,0),
(14846,17,12248,1,3600,0,1,NULL,0,0,0),
(14846,18,12251,1,3600,0,1,NULL,0,0,0),
(14846,19,12810,2,43200,0,1,NULL,0,0,0),
(14846,20,13463,3,43200,0,1,NULL,0,0,0),
(14846,21,13464,3,43200,0,1,NULL,0,0,0),
(14846,22,19302,1,43200,0,1,NULL,0,0,0),
(14846,23,19303,1,43200,0,1,NULL,0,0,0);
