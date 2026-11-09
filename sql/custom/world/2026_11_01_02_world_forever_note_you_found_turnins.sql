-- "... and that note you found" (Cozy Sleeping Bag chain, Discord report sudoge 11-01): neither side could hand in the first quest.
-- The turn-in objects were swapped between the two faction versions:
--   79007 (Horde) starts at Burned-Out Remains 415106 (The Barrens, -3380 -2079) but was handed in at 415107 / Nailed Plank 424010,
--         which stand only in Westfall (-10581 1708).
--   79008 (Alliance) starts at Burned-Out Remains 415107 (Westfall) but was handed in at 415106 / Nailed Plank 417072,
--         which stand only in The Barrens.
-- Each quest now ends at the remains and plank beside its own start. Spawns, spells and the rest of the chain are unchanged:
-- Stepping Stones 79192 -> Scramble 79980 -> Wet Job 79974 -> Eagle's Fist 79975 -> This Must Be The Place 79976 (Cozy Sleeping Bag).
DELETE FROM `gameobject_questender` WHERE `quest` IN (79007, 79008);
INSERT INTO `gameobject_questender` (`id`, `quest`, `VerifiedBuild`) VALUES
(415106, 79007, 0),
(417072, 79007, 0),
(415107, 79008, 0),
(424010, 79008, 0);
-- The later steps had no "next quest", so the following quest did not pop up at the object that ends the previous one.
UPDATE `quest_template` SET `RewardNextQuest` = 79974 WHERE `ID` = 79980 AND `RewardNextQuest` = 0;
UPDATE `quest_template` SET `RewardNextQuest` = 79975 WHERE `ID` = 79974 AND `RewardNextQuest` = 0;
UPDATE `quest_template` SET `RewardNextQuest` = 79976 WHERE `ID` = 79975 AND `RewardNextQuest` = 0;
