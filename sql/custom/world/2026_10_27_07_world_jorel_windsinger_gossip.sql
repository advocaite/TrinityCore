-- The Wounds of Betrayal (94489): Jorel Windsinger (258130) could not be talked to (Discord, KailON). The sniff gave him npcflag 0,
-- and WorldSession::HandleGossipHelloOpcode needs UNIT_NPC_FLAG_GOSSIP, so his menu 41923 ("<Leave to tend to the other druids.>",
-- the Find Jorel Windsinger credit, 2026_10_05_01) never opened. He is the only spawned creature with a scripted menu and neither
-- the gossip nor the quest giver flag. The other Forever server has npcflag 1 on him.
UPDATE `creature_template` SET `npcflag` = `npcflag` | 1 WHERE `entry` = 258130; -- UNIT_NPC_FLAG_GOSSIP

-- The conditions of seven Forever gossip options used the GossipOptionID as SourceEntry; ConditionMgr::addToGossipMenuItems matches
-- the OptionID (all 0 here), so they never attached and the options showed after the objective was done too.
UPDATE `conditions` SET `SourceEntry` = 0 WHERE `SourceTypeOrReferenceId` = 15 AND (`SourceGroup`, `SourceEntry`) IN
((40780,136302),(40962,136544),(40964,136549),(41313,137096),(41553,137326),(41923,137859),(42169,140111));
