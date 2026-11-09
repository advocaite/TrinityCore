-- Earthseer Farsen (264936): Farsen's Watch (96392) could not be done. His far sight option never showed (Discord, Neylem).
-- Player::GetGossipMenuForSource only picks a creature_template_gossip menu that has a gossip_menu row, and menu 2649360 had
-- none (2026_10_24_50 added only its option), so the client got menu 0: the quest list without the option. He is the only
-- spawned creature with such a menu. The greeting (npc_text, broadcast text 400000 in hotfixes 2026_10_27_00) and the option
-- text are the official server's, from Neylem's screenshot. The option itself, its conditions and the SAI (cast Farsen's Far
-- Sight, credit 264936, close) stay as they were. Safe to run again.
DELETE FROM `npc_text` WHERE `ID` = 2649360;
INSERT INTO `npc_text` (`ID`,`Probability0`,`Probability1`,`Probability2`,`Probability3`,`Probability4`,`Probability5`,`Probability6`,`Probability7`,`BroadcastTextID0`,`BroadcastTextID1`,`BroadcastTextID2`,`BroadcastTextID3`,`BroadcastTextID4`,`BroadcastTextID5`,`BroadcastTextID6`,`BroadcastTextID7`,`VerifiedBuild`) VALUES
(2649360,1,0,0,0,0,0,0,0,400000,0,0,0,0,0,0,0,0);
DELETE FROM `gossip_menu` WHERE `MenuID` = 2649360;
INSERT INTO `gossip_menu` (`MenuID`,`TextID`,`VerifiedBuild`) VALUES
(2649360,2649360,0);
UPDATE `gossip_menu_option` SET `OptionText` = 'Earthseer, please share your farsight with me.' WHERE `MenuID` = 2649360 AND `OptionID` = 0;
