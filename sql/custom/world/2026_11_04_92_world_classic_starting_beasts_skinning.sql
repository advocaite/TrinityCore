-- Skinning: the starting-zone beasts (Mottled Boar, Scorpid Worker, Felstalker of the Valley of Trials, Young Thistle Boar of
-- Shadowglen, Plainstrider of Mulgore) had no skinning loot, so they could not be skinned. They give Ruined Leather Scraps (2934)
-- again, as in vanilla. Lar'korwi (Un'Goro) pointed at an empty skinning loot list and now gives Rugged Leather (8170).
-- Only the original vanilla entries are touched (the later copies with the same names are other NPCs). Safe to run again.
UPDATE `creature_template_difficulty` SET `SkinLootID` = `Entry` WHERE `Entry` IN (1984, 2955, 3098, 3102, 3124, 9684) AND `DifficultyID` = 0;
DELETE FROM `skinning_loot_template` WHERE `Entry` IN (1984, 2955, 3098, 3102, 3124, 9684);
INSERT INTO `skinning_loot_template` (`Entry`, `ItemType`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(1984, 0, 2934, 100, 0, 1, 0, 1, 1, 'Young Thistle Boar - Ruined Leather Scraps'),
(2955, 0, 2934, 100, 0, 1, 0, 1, 1, 'Plainstrider - Ruined Leather Scraps'),
(3098, 0, 2934, 100, 0, 1, 0, 1, 1, 'Mottled Boar - Ruined Leather Scraps'),
(3102, 0, 2934, 100, 0, 1, 0, 1, 1, 'Felstalker - Ruined Leather Scraps'),
(3124, 0, 2934, 100, 0, 1, 0, 1, 1, 'Scorpid Worker - Ruined Leather Scraps'),
(9684, 0, 8170, 100, 0, 1, 0, 1, 1, 'Lar''korwi - Rugged Leather');
