-- Fixes from the Questie project's hand-written Forever corrections (Questie team, Questie/QuestieDB,
-- src/corrections/Forever, master c1b50fef, 2026-11-08, GPLv3), taken from what players saw in the live game.
-- Already matching here: Stepping Stones (79192) after either note quest, the 79980 -> 79974 -> 79975 -> 79976 chain,
-- Favorite Food (97249) after Meal Appeal (97246), Knowledge in the Deeps (971) from level 18, and the new object spawns.
-- Safe to run again.

-- 1) "... and that note you found" (79007 Barrens, 79008 Westfall) sends you to the other burned-out camp: 79007 starts at the
--    Barrens Burned-Out Remains (415106) and is handed in at the Westfall one (415107), 79008 the other way round. Stepping
--    Stones (79192) starts only at the Nailed Planks (417072 Barrens, 424010 Westfall), which hand in nothing. Ours (from the
--    AnoCore port) handed each note in at its own camp or its plank and also started 79192 at the remains.
DELETE FROM `gameobject_queststarter` WHERE `quest` IN (79007,79008,79192) AND `id` IN (415106,415107,417072,424010);
INSERT INTO `gameobject_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES
(415106,79007,0),
(415107,79008,0),
(417072,79192,0),
(424010,79192,0);
DELETE FROM `gameobject_questender` WHERE `quest` IN (79007,79008) AND `id` IN (415106,415107,417072,424010);
INSERT INTO `gameobject_questender` (`id`,`quest`,`VerifiedBuild`) VALUES
(415107,79007,0),
(415106,79008,0);

-- 2) The Sisterhood of Elune (98391, Darnassus) is a breadcrumb for The Temple of the Moon (2519), as on the live realm:
--    it goes away once 2519 is taken, so it can't be picked up after the Temple chain (Tears of the Moon 2518) has started.
UPDATE `quest_template_addon` SET `BreadcrumbForQuestId`=2519 WHERE `ID`=98391;

