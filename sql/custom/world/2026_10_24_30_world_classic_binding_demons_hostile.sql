-- Classic 1.60 (WoW Forever): Discord report "Voidwalker summon for the Warlock quest is friendly, so can't attack it to complete the quest."
--
-- The Binding (Voidwalker, quests 1689 Stormwind and 1504 Orgrimmar): the Bloodstone Choker / Glyphs of Summoning cast Summon Voidwalker
-- (7728 / 9222), whose send event (1131 / 1787) runs event_scripts command 10 and summons Summoned Voidwalker 5676 with no summon
-- properties, so it keeps its creature_template faction. That faction is 35 (friendly to everyone) in the base world database, which
-- relied on a SmartAI row ("after 1 s: set faction 14") to turn it hostile. The forever baseline replaced all smart_scripts with the
-- VMaNGOS conversion, which has no faction row because VMaNGOS keeps the hostile faction on the template itself, so the demon now stays
-- friendly and can never be attacked.
--
-- Summoned Felhunter 6268 (The Binding, quest 1795, at Strahad's summoning circle) has the same gap: faction 35 in the base world database, the base 2 s
-- "set faction 14" row is gone, and its SmartAI script "start attacking closest player" cannot attack while friendly.
--
-- Use the VMaNGOS template factions: Summoned Voidwalker 14, Summoned Felhunter 90 (Summoned Succubus 5677 already has 90).

UPDATE `creature_template` SET `faction` = 14 WHERE `entry` = 5676;
UPDATE `creature_template` SET `faction` = 90 WHERE `entry` = 6268;
