-- "A Sealed Crate" (91899, 91900, 91904, 91905): turn in a Sealed crate to Marcy Baker <Inventory Intake> (256390, Lakeshire)
-- or Dokimi <Inventory Intake> (256386, the Crossroads) for Merchant's Favor. Lifts the hold in 2026_11_05_71.
-- Players can now fill Waylaid Crates (2026_11_04_50), but nobody took the Sealed crates in, because the four quests had no
-- objectives (free Favor) and were held. Now each quest takes one Sealed crate, is offered only while you carry that crate,
-- and can be done again for every crate.
-- Which crate goes with which quest: the player guides give 5 Favor for an Apprentice crate and 10 for a Journeyman one,
-- doubled for the green crates (crafted goods). That matches the four rewards (5 / 10 / 10 / 20):
--   91899  5 Favor  Sealed Apprentice Crate (white) 248765
--   91900 10 Favor  Sealed Apprentice Crate (green) 248770
--   91904 10 Favor  Sealed Journeyman Crate (white) 248766
--   91905 20 Favor  Sealed Journeyman Crate (green) 248771
-- 91900 and 91904 may be the other way round on live; the reward is the same, so players see no difference.
-- Not covered yet (no quest for them in our data): blue crates and Expert / Artisan / Elite crates.
-- Safe to run again.

-- required crate
DELETE FROM `quest_objectives` WHERE `QuestID` IN (91899, 91900, 91904, 91905);
DELETE FROM `quest_objectives` WHERE `ID` IN (9189900, 9190000, 9190400, 9190500);
INSERT INTO `quest_objectives` (`ID`, `QuestID`, `Type`, `Order`, `StorageIndex`, `ObjectID`, `Amount`, `Flags`, `Visible`, `Description`, `VerifiedBuild`) VALUES
(9189900, 91899, 1, 0, 0, 248765, 1, 0, 1, NULL, 0),
(9190000, 91900, 1, 0, 0, 248770, 1, 0, 1, NULL, 0),
(9190400, 91904, 1, 0, 0, 248766, 1, 0, 1, NULL, 0),
(9190500, 91905, 1, 0, 0, 248771, 1, 0, 1, NULL, 0);

-- repeatable (one turn-in per crate)
INSERT INTO `quest_template_addon` (`ID`, `SpecialFlags`) VALUES
(91899, 1), (91900, 1), (91904, 1), (91905, 1)
ON DUPLICATE KEY UPDATE `SpecialFlags` = `SpecialFlags` | 1;

-- offered only while the player carries the matching crate
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 19 AND `SourceEntry` IN (91899, 91900, 91904, 91905) AND `ConditionTypeOrReference` = 2;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`, `SourceGroup`, `SourceEntry`, `SourceId`, `ElseGroup`, `ConditionTypeOrReference`, `ConditionTarget`, `ConditionValue1`, `ConditionValue2`, `ConditionValue3`, `NegativeCondition`, `ErrorType`, `ErrorTextId`, `ScriptName`, `Comment`) VALUES
(19, 0, 91899, 0, 0, 2, 0, 248765, 1, 0, 0, 0, 0, '', 'A Sealed Crate (5 Favor) - only with a Sealed Apprentice Crate (white)'),
(19, 0, 91900, 0, 0, 2, 0, 248770, 1, 0, 0, 0, 0, '', 'A Sealed Crate (10 Favor) - only with a Sealed Apprentice Crate (green)'),
(19, 0, 91904, 0, 0, 2, 0, 248766, 1, 0, 0, 0, 0, '', 'A Sealed Crate (10 Favor) - only with a Sealed Journeyman Crate (white)'),
(19, 0, 91905, 0, 0, 2, 0, 248771, 1, 0, 0, 0, 0, '', 'A Sealed Crate (20 Favor) - only with a Sealed Journeyman Crate (green)');

-- both depot NPCs offer and take them
DELETE FROM `creature_queststarter` WHERE `id` IN (256386, 256390) AND `quest` IN (91899, 91900, 91904, 91905);
INSERT INTO `creature_queststarter` (`id`, `quest`, `VerifiedBuild`) VALUES
(256386, 91899, 0), (256386, 91900, 0), (256386, 91904, 0), (256386, 91905, 0),
(256390, 91899, 0), (256390, 91900, 0), (256390, 91904, 0), (256390, 91905, 0);
DELETE FROM `creature_questender` WHERE `id` IN (256386, 256390) AND `quest` IN (91899, 91900, 91904, 91905);
INSERT INTO `creature_questender` (`id`, `quest`, `VerifiedBuild`) VALUES
(256386, 91899, 0), (256386, 91900, 0), (256386, 91904, 0), (256386, 91905, 0),
(256390, 91899, 0), (256390, 91900, 0), (256390, 91904, 0), (256390, 91905, 0);
UPDATE `creature_template` SET `npcflag` = `npcflag` | 2 WHERE `entry` IN (256386, 256390);
