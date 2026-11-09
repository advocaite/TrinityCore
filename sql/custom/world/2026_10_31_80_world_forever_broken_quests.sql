-- Broken quests found by a full check of the 0.1.28 world data (10-27 dump plus every later sql/custom/world file),
-- against the 1.60 client data (70532 and 70685 DB2). Only fixes that need no data we don't already hold.

-- 1) Reward choices missing on 150 quests (Harvesting Windstones 93552, Wanted: Hogger 176 and 148 more).
-- QuestPackageID holds a value the sniff import put there, but the 1.60 client has no quest packages at all
-- (QuestPackageItem.db2 has 0 rows in 70532 and in 70685). With a package id the client looks for package items
-- instead of the quest's own choices, shows none and completes the quest with item 0.
-- Clearing the column fixes it with no build, and also fixes the quest query answer the client caches.
-- The server has no package items to give either, so nothing else changes. Same cause as windstones-reward-choice-fix.patch.
UPDATE `quest_template` SET `QuestPackageID` = 0 WHERE `QuestPackageID` <> 0;

-- 2) Thundering Hearts (97985, Horde, Thousand Needles): Stormheart 270094 never drops Stormheart's Still-beating Heart 279186.
-- The loot rows exist under entry 270094, but the creature's difficulty row has LootID 0, so it drops nothing.
-- It is the only creature in the world with loot rows under its own entry and no LootID pointing at them.
UPDATE `creature_template_difficulty` SET `LootID` = 270094 WHERE `Entry` = 270094 AND `DifficultyID` = 0 AND `LootID` = 0;
