-- Skyborne start zone (Zephras Isle) report by sombi4, 11-06: quest items never reached the bags, and the
-- objectives did not count, on Harvesting Windstones 93552, Aggressive Encroachment 92473, Foul Matriarch 92470,
-- The Problem With Prideclaws 92515, A Little Beauty 93951, Restocking the Larders 92553, WANTED: Vulgara 93318,
-- The Criminal Element 92517, Stolen Supplies 92551, Havoc in the Highlands 92550 and A Last Request 93927.
-- Cause: these item objectives have Flags2 bit 1 (quest-bound item, from the 70124 sniff). With that bit the core never
-- creates the item; it only bumps a counter in the quest log. The 1.60 client keeps quest items in the bags like
-- classic does, so the counter did not show and nothing appeared. Clearing the bit makes the looted item go to the
-- bags and count from there, as on the vanilla quests. Same bit on every other item objective (186 on live quests,
-- the rest retail rows nobody can take), so it is cleared everywhere.
-- Bugged 93949 (Valanaar, kill 8 Enchanted Skyhoppers): its credit creature 257066 has no template and nothing gives
-- it, so kills stayed at 0/8. The objective now counts Skyhopper 251727, and the second Skyhopper template 251314
-- (spawned in the same place) gives the same credit.
-- Found and tested by sombi4 (Discord #bug-report, 11-06). Safe to run twice.
UPDATE `quest_objectives` SET `Flags2` = `Flags2` & ~1 WHERE `Type` = 1 AND (`Flags2` & 1) <> 0;
UPDATE `quest_objectives` SET `ObjectID` = 251727 WHERE `ID` = 468000 AND `QuestID` = 93949 AND `ObjectID` = 257066;
UPDATE `creature_template` SET `KillCredit1` = 251727 WHERE `entry` = 251314 AND `KillCredit1` = 0;
