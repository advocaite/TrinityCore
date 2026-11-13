-- Goldshire: Jason Mathers (383, Fish and Tackle Merchant) sells fish as well as fishing gear (fish list found by SIN).
-- 2026_11_12_23 gave him the vanilla fishing supplier list (slots 1-9). The Forever quest Baited for Success (99131) has him selling
-- fish too, so he also gets the six fish Hal McAllister (3540) sells, at their normal price and unlimited. INFERRED: his exact
-- Forever list is not known. Safe to run again.
DELETE FROM `npc_vendor` WHERE `entry` = 383 AND `item` IN (787,4592,4593,4594,21552,8957);
INSERT INTO `npc_vendor` (`entry`,`slot`,`item`,`maxcount`,`incrtime`,`ExtendedCost`,`type`,`BonusListIDs`,`PlayerConditionID`,`IgnoreFiltering`,`VerifiedBuild`) VALUES
(383,10,787,0,0,0,1,NULL,0,0,0),
(383,11,4592,0,0,0,1,NULL,0,0,0),
(383,12,4593,0,0,0,1,NULL,0,0,0),
(383,13,4594,0,0,0,1,NULL,0,0,0),
(383,14,21552,0,0,0,1,NULL,0,0,0),
(383,15,8957,0,0,0,1,NULL,0,0,0);
