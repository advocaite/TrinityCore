-- Creatures that were friendly to everyone (faction template 35) but are enemies on WoW Forever: Wowhead Forever shows them hostile
-- and the other Forever server world gives them a hostile faction.
-- Encroaching Soldier and Outraged Pillager (Dustwallow border, Theramore soldiers) are the kill targets of the Horde quest
-- Wrongly Blamed, Justly Corrected (97250), which could not be finished. Highland Lurker (Wetlands spider) and Elder Wendigo
-- (Dun Morogh) are plain enemies.
UPDATE `creature_template` SET `faction`=122 WHERE `entry` IN (268622,268623) AND `faction`=35;
UPDATE `creature_template` SET `faction`=22 WHERE `entry`=260804 AND `faction`=35;
UPDATE `creature_template` SET `faction`=37 WHERE `entry`=271530 AND `faction`=35;
