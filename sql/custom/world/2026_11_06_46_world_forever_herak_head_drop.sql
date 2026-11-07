-- Get Off Our Land (99080): the Head of Herak (286075) is the quest's third objective, but Herak the Pillager (275767) dropped it
-- only 40% of the time (a guessed chance: the drop was never seen in the sniffs). He is a single named camp leader, so a kill
-- without the head left the quest stuck (player report 2026-11-06). Always drops for players on the quest.
UPDATE `creature_loot_template` SET `Chance` = 100 WHERE `Entry` = 275767 AND `ItemType` = 0 AND `Item` = 286075;
