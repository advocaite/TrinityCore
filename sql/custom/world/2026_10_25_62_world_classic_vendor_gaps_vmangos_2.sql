-- Classic 1.60: the rest of the vendor comparison with VMaNGOS 1.12 (vendor templates included, only items the 70532 client has):
-- the five Smokywood Pastures vendors in the capitals lacked Mistletoe 17195 and Candy Cane 17344, Ranik (Booty Bay) the
-- limited Crawler Claw 2675 and Meilosh (Timbermaw Hold) the limited pattern 15742. Stock and restock as in VMaNGOS.
-- Left out: the pre-1.12 PvP rank sets of Captain Dirgehammer and Lady Palanseer (replaced in 1.12) and Innkeeper Thulbek's
-- condition-gated Hunter's Boomstick. Safe to run again.

DELETE FROM `npc_vendor` WHERE (`entry` IN (13420,13429,13432,13433,13435) AND `item` IN (17195,17344))
    OR (`entry` = 3499 AND `item` = 2675) OR (`entry` = 11557 AND `item` = 15742);
INSERT INTO `npc_vendor` (`entry`,`slot`,`item`,`maxcount`,`incrtime`,`ExtendedCost`,`type`,`BonusListIDs`,`PlayerConditionID`,`IgnoreFiltering`,`VerifiedBuild`) VALUES
(13420,0,17195,0,0,0,1,NULL,0,0,0),(13420,0,17344,0,0,0,1,NULL,0,0,0),
(13429,0,17195,0,0,0,1,NULL,0,0,0),(13429,0,17344,0,0,0,1,NULL,0,0,0),
(13432,0,17195,0,0,0,1,NULL,0,0,0),(13432,0,17344,0,0,0,1,NULL,0,0,0),
(13433,0,17195,0,0,0,1,NULL,0,0,0),(13433,0,17344,0,0,0,1,NULL,0,0,0),
(13435,0,17195,0,0,0,1,NULL,0,0,0),(13435,0,17344,0,0,0,1,NULL,0,0,0),
(3499,0,2675,1,7200,0,1,NULL,0,0,0),
(11557,0,15742,1,7200,0,1,NULL,0,0,0);
