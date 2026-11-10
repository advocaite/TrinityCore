-- Classic 1.60 (WoW Forever): spawned quest starters without the QUESTGIVER npcflag (server audit 2026-10-24, finding 6).
--
-- Six creatures start an enabled quest (creature_queststarter from the Forever sniff) but carry no quest-giver flag, so the client
-- never shows the "!" and the quest cannot be taken:
--   2756   Chao-Ju                 97277 Grund and Gozwin          npcflag 0
--   3348   Kor'geld                97242 Yelmak's Medley           npcflag 129 (gossip + vendor)
--   251371 Falorne Fallwind        92597 Reading the Ley Lines     npcflag 1
--   259084 Denaaris Stargale       94947 Welcome to Azeroth        npcflag 0
--   271613 Unfinished Abomination  97290 Unending Torment          npcflag 0
--   275954 Bareth Dawnstone        99144 Seeking Refuge            npcflag 0
-- INFERRED: the sniff has the starter rows but no flag; live Forever may open these through gossip instead. Harmless if so.
--
-- The 36 quest givers that are never spawned (Felwood Ancients, Xiggs Fuselighter, Thund, Acride, Windsor's cell, Arei ...) have no
-- spawn row in VMaNGOS either: every one is summoned by an item, a spell or a script there, so no spawn is added here.

UPDATE `creature_template` SET `npcflag` = `npcflag` | 2 WHERE `entry` IN (2756, 3348, 251371, 259084, 271613, 275954) AND (`npcflag` & 2) = 0;
