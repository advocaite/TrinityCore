-- Brakk (265809, Durotar 52,47), Kaga Wildhoof (265810, Mulgore) and Lyreena Duskblade (265811, Teldrassil), the camping
-- adventurers who take The Adventurer and give The Great Outdoors, could go missing (Discord report: Brakk not at his camp).
-- They came in without sniffed flags (10-26_31): unit_flags 0, so hostile creatures could fight them. Brakk's camp sits at
-- the edge of the Ridgeshade Lurker spawns (one 18 yards away), so a lurker can kill him and he is gone until he respawns
-- (cause INFERRED, no log). Every sniffed adventurer (Sam Sarsaparilla 263399, Raan Wildwind 263664, Eleanor Shackleton
-- 265812, Eric Brighthammer 265813) has UNIT_FLAG_IMMUNE_TO_NPC, quest giver + gossip and the shared menu 42965; these
-- three now match them.
UPDATE `creature_template` SET `unit_flags` = `unit_flags` | 0x200, `npcflag` = `npcflag` | 0x3 WHERE `entry` IN (265809, 265810, 265811);
DELETE FROM `creature_template_gossip` WHERE `CreatureID` IN (265809, 265810, 265811);
INSERT INTO `creature_template_gossip` (`CreatureID`, `MenuID`, `VerifiedBuild`) VALUES
(265809, 42965, 0),
(265810, 42965, 0),
(265811, 42965, 0);
