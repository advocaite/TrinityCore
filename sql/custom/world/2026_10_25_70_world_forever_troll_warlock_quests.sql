-- Classic 1.60 (WoW Forever): Troll warlocks get their Valley of Trials class quests (Discord: "Vile Familiars is not there, can't get my imp").
--
-- Forever lets Trolls be warlocks (playercreateinfo race 8 class 9, start in the Valley of Trials), but the warlock quests there
-- still carry the vanilla race masks: Orc and Undead (18) for Ruzan's Vile Familiars, Orc only (2) for Gornek's Tainted Parchment.
-- Ruzan (5765) is spawned next to Mai'ah, but a Troll warlock sees no quest on him and never gets the imp (RewardSpell 7763).
-- Add the Troll bit (128) to the three quests. Human, Gnome, Orc and Undead warlock quests already cover their races.
UPDATE `quest_template` SET `AllowableRaces` = `AllowableRaces` | 128 WHERE `ID` IN (1485, 1499, 3090);
