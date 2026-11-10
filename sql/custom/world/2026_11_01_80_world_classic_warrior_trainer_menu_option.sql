-- Warrior trainers Seena Skybreaker (252377), Corsan Earthrazer (254088) and Blademaster Ren (251964) use gossip menu 41126.
-- That menu has a text but no options, so the client never offers "I'd like training!" although the trainers have their
-- trainer flags and lists. Give it the trainer option the other class trainer menus carry (41123, Quel'ana Quickgale).
-- GossipOptionID 136809 is not sniffed: it is the next free id after 41123's 136808 and is used by no other menu.
-- Found by Trixx34 (Trixx34/Trinity-Forever-Modules, GPL-2.0). Safe to run again.
DELETE FROM `gossip_menu_option` WHERE `MenuID` = 41126 AND `OptionID` = 0;
INSERT INTO `gossip_menu_option` (`MenuID`, `GossipOptionID`, `OptionID`, `OptionNpc`, `OptionText`, `OptionBroadcastTextID`, `Language`, `Flags`,
    `ActionMenuID`, `ActionPoiID`, `GossipNpcOptionID`, `BoxCoded`, `BoxMoney`, `BoxText`, `BoxBroadcastTextID`, `SpellID`, `OverrideIconID`, `VerifiedBuild`)
SELECT 41126, 136809, 0, 3, 'I\'d like training!', 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 0
FROM DUAL WHERE NOT EXISTS (SELECT 1 FROM `gossip_menu_option` WHERE `GossipOptionID` = 136809);
