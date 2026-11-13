-- Remember That I Love You (92415) could not be picked up (player report 2026-11-13, Discord "bugs 2" item 2). The Crumpled Paper
-- (gameobject 572922) lying beside Edward Heartweaver's body in the Ruins of Lordaeron is a quest giver object whose use condition
-- (PlayerCondition 924) is exactly the quest's Alliance race mask (6130900294268439629), and accepting the quest hands out the
-- Blood-Stained Letter (251522, the quest's StartItem). Our quest givers came from a Horde character's sniff, who could not use the
-- paper, so it had no quest and the quest had no Alliance giver at all.
-- INFERRED: the paper starts 92415 (same race mask, same spot as the story, no other quest fits). The two items that also start it
-- in the client data (251270, 251520) have no known source.
-- Safe to run again.

DELETE FROM `gameobject_queststarter` WHERE `id` = 572922 AND `quest` = 92415;
INSERT INTO `gameobject_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (572922,92415,0);
