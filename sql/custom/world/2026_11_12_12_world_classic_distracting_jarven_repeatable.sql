-- Bitter Rivals (310, Kharanos; kuanarin, 0.1.35): with Bitter Rivals, the keg and a Thunder Ale in the bags, neither the Guarded
-- Thunder Ale Barrel nor Jarven Thunderbrew offers anything, so the Unguarded barrel where 310 is handed in never comes.
-- Our Distracting Jarven (308) is a one-time quest that needs Guarded Thunderbrew Barrel (403) done first. A player who
-- handed Jarven his ale before 2026_11_09_20 (the barrels did not swap then), or who missed the two minutes the Unguarded
-- barrel stands, has 403 and 308 done for good and can never reach the barrel again.
-- AzerothCore (data/sql/base/db_world quest_template_addon) has 308 offered while Bitter Rivals is in the log
-- (PrevQuestID -310) and repeatable (SpecialFlags 1); so it is here. 403 stays as it is. Safe to run again.
UPDATE `quest_template_addon` SET `PrevQuestID`=-310, `SpecialFlags`=`SpecialFlags`|1 WHERE `ID`=308;
