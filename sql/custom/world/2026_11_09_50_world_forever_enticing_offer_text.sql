-- WoW Forever: An Enticing Offer (99130). Our quest log line was the short placeholder "Collect ingredients for murloc bait.".
-- The live realm's text (player cache of 2026-11-09, build 70862) names the items and the turn-in, Remy "Two Times" in Goldshire,
-- which also confirms the turn-in we had only inferred (2026_10_24_61). Item counts written as numbers, like our other quest lines:
-- 6 Vials of Animal Blood (286215, objective 479454) and 18 Duskweed Petals (286214, objective 479453).
-- Safe to run again.
UPDATE `quest_template` SET `LogDescription`='Collect 6 Vials of Animal Blood and 18 Duskweed Petals for murloc bait, then return to Remy "Two Times" in Goldshire.' WHERE `ID`=99130;
