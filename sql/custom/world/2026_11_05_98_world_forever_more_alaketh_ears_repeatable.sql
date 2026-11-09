-- "More Al'Aketh Ears" (93459, Zephris Isle, Vayn Moongaze 254151) is the repeatable turn-in that follows "Mercy Falls on
-- Deaf Ears" (93165), but it had no repeatable flag, so after the first turn-in it was never offered again (player report
-- 2026-11-05). Forever quests imported from sniffs carry no server-side SpecialFlags. It is the only Forever-era quest in the
-- world data with the "same objective as the quest before it" repeat pattern that lacks the flag.
-- Safe to run again.
UPDATE `quest_template_addon` SET `SpecialFlags` = `SpecialFlags` | 1 WHERE `ID` = 93459;
