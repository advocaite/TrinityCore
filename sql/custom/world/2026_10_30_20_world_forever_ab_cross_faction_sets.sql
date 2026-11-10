-- Arathi Basin quartermasters sell Forever's cross-faction sets.
-- Forever lets Alliance play Shaman and Horde play Paladin, and adds the matching League of Arathor / Defilers sets: Highlander's
-- Mail (Alliance mail for Shamans) and Defiler's Lamellar (Horde plate for Paladins), all four level brackets. The items are in the
-- client (ItemSparse 70685, same reputation gates and prices as the other AB pieces), but our vendors never listed them.
-- Found by comparing our vendors against the reputation sources in wowsims/forever (MIT, https://github.com/wowsims/forever).
-- Rows copy the other AB pieces on the same vendor (gold price from the item, no extended cost). Safe to run again.
DELETE FROM `npc_vendor` WHERE `entry`=15127 AND `item` IN (20044,20051,20056,20118,20119,20120,20121,20122,20123);
INSERT INTO `npc_vendor` (`entry`,`slot`,`item`,`maxcount`,`incrtime`,`ExtendedCost`,`type`,`BonusListIDs`,`PlayerConditionID`,`IgnoreFiltering`,`VerifiedBuild`) VALUES
(15127,0,20044,0,0,0,1,NULL,0,0,0),
(15127,0,20051,0,0,0,1,NULL,0,0,0),
(15127,0,20056,0,0,0,1,NULL,0,0,0),
(15127,0,20118,0,0,0,1,NULL,0,0,0),
(15127,0,20119,0,0,0,1,NULL,0,0,0),
(15127,0,20120,0,0,0,1,NULL,0,0,0),
(15127,0,20121,0,0,0,1,NULL,0,0,0),
(15127,0,20122,0,0,0,1,NULL,0,0,0),
(15127,0,20123,0,0,0,1,NULL,0,0,0);
DELETE FROM `npc_vendor` WHERE `entry`=15126 AND `item` IN (20177,20178,20179,20180,20181,20182,20183,20184,20185);
INSERT INTO `npc_vendor` (`entry`,`slot`,`item`,`maxcount`,`incrtime`,`ExtendedCost`,`type`,`BonusListIDs`,`PlayerConditionID`,`IgnoreFiltering`,`VerifiedBuild`) VALUES
(15126,0,20177,0,0,0,1,NULL,0,0,0),
(15126,0,20178,0,0,0,1,NULL,0,0,0),
(15126,0,20179,0,0,0,1,NULL,0,0,0),
(15126,0,20180,0,0,0,1,NULL,0,0,0),
(15126,0,20181,0,0,0,1,NULL,0,0,0),
(15126,0,20182,0,0,0,1,NULL,0,0,0),
(15126,0,20183,0,0,0,1,NULL,0,0,0),
(15126,0,20184,0,0,0,1,NULL,0,0,0),
(15126,0,20185,0,0,0,1,NULL,0,0,0);
