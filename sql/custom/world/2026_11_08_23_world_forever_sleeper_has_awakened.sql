-- The Sleeper Has Awakened (5321, Darkshore): the live quest has no objectives in the log; it completes when Kerlonian
-- Evershade reaches Liladris Moonriver (the escort script calls the quest event). Ours carried a "Horn of Awakening (13536)
-- 0/1" item objective from the vanilla conversion. The horn still comes from Kerlonian's Chest next to him and is still
-- what wakes him; it is just not a quest item to hand in. Questie project's Forever corrections (Questie team,
-- Questie/QuestieDB, master 3a621efa, 2026-11-08, GPLv3): objectives empty, escort south from his bag. Safe to run again.
DELETE FROM `quest_objectives` WHERE `QuestID`=5321 AND `Type`=1 AND `ObjectID`=13536;
