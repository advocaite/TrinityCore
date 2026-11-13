-- Scripted Classic NPCs with the talk flag can be talked to again (Discord 2026-11-12, guard59232: in Zul'Farrak Sergeant Bly is
-- friendly but has no dialogue, so the Divino-matic Rod for 2768 cannot be got).
-- The server sends a creature's NPC flags to each player through Player::CanSeeGossipOn, which drops the talk flag when the creature
-- has no gossip menu (creature_template_gossip plus a gossip_menu row whose npc_text exists). These ten creatures have a Classic
-- script that builds the whole menu in OnGossipHello, but the baseline (2026_09_27_00) left them without a menu, so no client could
-- open their talk window. Same fault and fix as the hire brokers (2026_11_01_20).
-- Each gets menu 91000000 + entry (the baseline's own numbering for Forever menus) with the text its script or vanilla shows first
-- (menu and text numbers as in AzerothCore creature_template / gossip_menu). The scripts still choose the text and the options;
-- the menu only lets the client see the flag, and gives the text to the scripts that ask GetGossipTextId.
--   7604 Sergeant Bly (Zul'Farrak, the pyramid event and the fight for the rod) 1515, 7607 Weegli Blastfuse (opens the end door) 1511,
--   13020 Vaelastrasz the Corrupt (BWL, starts the fight) 7156, 7998 Blastmaster Emi Shortfuse (Gnomeregan, Grubbis event) 1693,
--   6669 The Threshwackonator 4100 (Darkshore, 2078) 718, 16365 Master Craftsman Omarion (Naxxramas patterns) 8506,
--   15612 Krug Skullsplit (Silithus, Field Duty) 8059, 14529 Franklin the Friendly and 14527 Simone the Inconspicuous (Stave of the
--   Ancients) 7043 and 7041, 17804 Squire Rowe (Stormwind Rendezvous / The Great Masquerade) 9063.
-- Scripted creatures with only the quest giver flag (Mikhail, Knot Thimblejack, Dying Archaeologist) or no flag (Plucky Johnson,
-- Klinfran the Crazed) are left as they are; their flags are the vanilla ones and need no talk flag. Safe to run again.
DELETE FROM `gossip_menu` WHERE `MenuID` IN (91007604,91007607,91013020,91007998,91006669,91016365,91015612,91014529,91014527,91017804);
INSERT INTO `gossip_menu` (`MenuID`,`TextID`,`VerifiedBuild`) VALUES
(91007604,1515,0),
(91007607,1511,0),
(91013020,7156,0),
(91007998,1693,0),
(91006669,718,0),
(91016365,8506,0),
(91015612,8059,0),
(91014529,7043,0),
(91014527,7041,0),
(91017804,9063,0);

DELETE FROM `creature_template_gossip` WHERE `CreatureID` IN (7604,7607,13020,7998,6669,16365,15612,14529,14527,17804);
INSERT INTO `creature_template_gossip` (`CreatureID`,`MenuID`,`VerifiedBuild`) VALUES
(7604,91007604,0),
(7607,91007607,0),
(13020,91013020,0),
(7998,91007998,0),
(6669,91006669,0),
(16365,91016365,0),
(15612,91015612,0),
(14529,91014529,0),
(14527,91014527,0),
(17804,91017804,0);
