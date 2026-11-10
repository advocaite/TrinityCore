-- Classic 1.60: vanilla quests locked behind Cataclysm and Burning Crusade quests nobody can get.
--
-- quest_template_addon.NextQuestID of a quest makes it one of the quests the next quest accepts as done before
-- (Player::SatisfyQuestDependentPreviousQuests). The retail rows below point at vanilla quests, but the retail quests
-- themselves are disabled or have no spawned giver here, so where they are the only such link the vanilla quest could
-- never be offered:
--   Clear the Way 5092 and Scarlet Diversions 5096 (the whole Western Plaguelands chain: cauldrons, All Along the
--   Watchtowers, Alas Andorhal and the Scholomance key), Harpy Raiders 867 (and Harpy Lieutenants, Serena Bloodfeather,
--   Letter to Jin'Zil after it), Disrupt the Attacks 871 and The Disruption Ends 872, Operation Recombobulation 412 and
--   Securing the Supply Lines 8280 (and the Silithus supply chain after it).
-- VMaNGOS 1.12 has none of these links (PrevQuestId 0, no quest names them as NextQuestId). The links of the same kind
-- that do not lock anything today (the OOX, Nathanos, tier 0.5 and Midsummer ones, where a vanilla quest also opens the
-- next one) are cleared too, so a later change cannot turn them into a lock. Safe to run again.

UPDATE `quest_template_addon` SET `NextQuestID` = 0 WHERE `ID` IN
(10373,10374,28877,29094,26642,28494,25667,28527,28528,28856,28859,
 25475,25476,14350,10492,10493,10494,10495,10496,10497,11933,11935) AND `NextQuestID` <> 0;
