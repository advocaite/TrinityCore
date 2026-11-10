-- Zephras Isle: quest drops that did not show on the creature's tooltip (Discord: Restocking the Larders).
-- The tooltip's quest line comes from the creature's quest item list (creature_questitem, sent with the creature query).
-- These creatures drop the item in our loot (Forever sniff and Wowhead Forever kills), but the list had no row for it,
-- so the drop worked while hovering the mob showed only its name and level.
--  92553 Restocking the Larders: Strider Meat 5469 (Galestrider), Small Egg 6889 (Galestrider, hippogryphs, Den'dralass,
--        Ur'endra, Juvenile Cloudrunner).
--  93165 Mercy Falls on Deaf Ears / 93459 More Al'Aketh Ears: item 258771 (seven Al'Aketh creatures).
--  92551 Stolen Supplies: item 252760 (Galemender Delanea, the only one of its droppers without the row).
-- Idx follows the rows these creatures already have. Clients that cached the creature need the Cache folder cleared once.
DELETE FROM `creature_questitem` WHERE `DifficultyID`=0 AND (`CreatureEntry`,`ItemId`) IN
((251661,5469),(251661,6889),(250936,6889),(251261,6889),(251284,6889),(251291,6889),(258443,6889),(271707,6889),
(252664,258771),(252665,258771),(252762,258771),(252763,258771),(252764,258771),(252765,258771),(252767,258771),
(259398,252760));
INSERT INTO `creature_questitem` (`CreatureEntry`,`DifficultyID`,`Idx`,`ItemId`,`VerifiedBuild`) VALUES
(251661,0,0,6889,0),
(251661,0,1,5469,0),
(250936,0,0,6889,0),
(251261,0,0,6889,0),
(251284,0,0,6889,0),
(251291,0,0,6889,0),
(258443,0,0,6889,0),
(271707,0,0,6889,0),
(252664,0,2,258771,0),
(252665,0,2,258771,0),
(252762,0,1,258771,0),
(252763,0,1,258771,0),
(252764,0,1,258771,0),
(252765,0,1,258771,0),
(252767,0,1,258771,0),
(259398,0,0,252760,0);
