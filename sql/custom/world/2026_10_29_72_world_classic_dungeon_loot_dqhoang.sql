-- Dungeon loot from Duy Hoang's (dqhoang) audit of the Horde-side dungeons (advocaite/TrinityCore PR #265):
--  * Mutanus the Devourer (3654, Wailing Caverns): Hypnotic Crystal 209838 at Wowhead Forever's rate (449 of 3,210 kills).
--  * Ribbly Screwspigot (9543, Blackrock Depths): Plans: Tempered Dark Iron Plate 227901 (518 of 2,105 kills).
--  * Gameobject 16577: its reference 12008 was listed twice in the baseline, so it rolled twice; one row is kept.
-- Safe to run again.
DELETE FROM `creature_loot_template` WHERE `Entry`=3654 AND `ItemType`=0 AND `Item`=209838;
DELETE FROM `creature_loot_template` WHERE `Entry`=9543 AND `ItemType`=0 AND `Item`=227901;
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(3654,0,209838,14,0,1,0,1,1,'Mutanus the Devourer: Hypnotic Crystal - Wowhead Forever 449/3210'),
(9543,0,227901,24.6,0,1,0,1,1,'Ribbly Screwspigot: Plans: Tempered Dark Iron Plate - Wowhead Forever 518/2105');
DELETE FROM `gameobject_loot_template` WHERE `Entry`=16577 AND `ItemType`=1 AND `Item`=12008;
INSERT INTO `gameobject_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(16577,1,12008,100,0,1,0,1,1,'VMaNGOS 1.12');
