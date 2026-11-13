-- Goldshire: Jason Mathers sells fishing gear again (Discord 2026-11-09, edward4488: "the angling merchant only offers quests and
-- won't let me trade. The trade menu is only available at the trainer, but it doesn't work." Lee Brown and Jason Mathers, Elwynn).
-- On Forever Jason Mathers 383 is the "Fish and Tackle Merchant" (his subtitle in the player's client, ruRU "Торговец рыбой и
-- снастями") and gives the Goldshire fishing quests; ours had only the quest giver flag and nothing to sell. Lee Brown 1651 is
-- the fishing trainer (vanilla flags 81, no vendor flag); the "I want to browse your goods" line in his menu 5665 is a retail
-- leftover that opened nothing, so it goes.
-- Jason gets the gossip and vendor flags, a menu like the other Forever vendors (91000000 + entry, npc_text 68, option "I want to
-- browse your goods.", broadcast text 3370) and the vanilla fishing supplier list (as Catherine Leland in Stormwind): Fishing Pole,
-- Shiny Bauble, Nightcrawlers, Bright Baubles, the three fishing recipes, Strong Fishing Pole and Aquadynamic Fish Attractor
-- (one each, restocked as vanilla). The exact Forever list is not known (INFERRED). Safe to run again.
UPDATE `creature_template` SET `npcflag` = `npcflag` | 1 | 128 WHERE `entry` = 383;

DELETE FROM `gossip_menu` WHERE `MenuID` = 91000383;
INSERT INTO `gossip_menu` (`MenuID`,`TextID`,`VerifiedBuild`) VALUES (91000383,68,0);
DELETE FROM `gossip_menu_option` WHERE `MenuID` = 91000383;
INSERT INTO `gossip_menu_option` (`MenuID`,`GossipOptionID`,`OptionID`,`OptionNpc`,`OptionText`,`OptionBroadcastTextID`,`Language`,`Flags`,`ActionMenuID`,`ActionPoiID`,`GossipNpcOptionID`,`BoxCoded`,`BoxMoney`,`BoxText`,`BoxBroadcastTextID`,`SpellID`,`OverrideIconID`,`VerifiedBuild`) VALUES
(91000383,94100383,1,1,'I want to browse your goods.',3370,0,0,0,0,NULL,0,0,'',0,NULL,NULL,0);
DELETE FROM `creature_template_gossip` WHERE `CreatureID` = 383;
INSERT INTO `creature_template_gossip` (`CreatureID`,`MenuID`,`VerifiedBuild`) VALUES (383,91000383,0);

DELETE FROM `npc_vendor` WHERE `entry` = 383;
INSERT INTO `npc_vendor` (`entry`,`slot`,`item`,`maxcount`,`incrtime`,`ExtendedCost`,`type`,`BonusListIDs`,`PlayerConditionID`,`IgnoreFiltering`,`VerifiedBuild`) VALUES
(383,1,6256,0,0,0,1,NULL,0,0,0),
(383,2,6529,0,0,0,1,NULL,0,0,0),
(383,3,6530,0,0,0,1,NULL,0,0,0),
(383,4,6532,0,0,0,1,NULL,0,0,0),
(383,5,6325,0,0,0,1,NULL,0,0,0),
(383,6,6330,0,0,0,1,NULL,0,0,0),
(383,7,6368,0,0,0,1,NULL,0,0,0),
(383,8,6365,1,9000,0,1,NULL,0,0,0),
(383,9,6533,1,43200,0,1,NULL,0,0,0);

DELETE FROM `gossip_menu_option` WHERE `MenuID` = 5665 AND `OptionNpc` = 1;
