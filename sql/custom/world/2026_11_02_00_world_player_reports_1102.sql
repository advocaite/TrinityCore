-- Player reports of 2026-11-02 (Azeldof, Onlybeams, Discord). Safe to run again.

-- The Affray (1719, warrior Berserker Stance): stepping into the ring (areatrigger 522) makes Twiggy Flathead start the event
-- (classic_at_twiggy_flathead). Nothing was bound to the trigger, so the event never started.
DELETE FROM `areatrigger_scripts` WHERE `entry` = 522;
INSERT INTO `areatrigger_scripts` (`entry`,`ScriptName`) VALUES (522,'classic_at_twiggy_flathead');

-- The Defias Brotherhood (155): finished by reaching the Moonbrook shed (areatrigger 78, areatrigger_involvedrelation). The
-- server only keeps that row for a quest with the area trigger completion flag (0x4), so the trigger did nothing.
UPDATE `quest_template` SET `Flags` = `Flags` | 0x4 WHERE `ID` = 155;

-- Bottles and Baubles (99143): Shiny Junk 286209 lies in the Junk Piles of the Elwynn murloc camp (gameobject 694785, nine spawns).
-- The murloc drops of 2026_10_24_63 were inferred while no source existed; the Junk Piles are the real one.
DELETE FROM `creature_loot_template` WHERE `Item` = 286209 AND `Entry` IN (285,735);
