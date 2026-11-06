-- Classic 1.60: Wailing Caverns bosses were missing their Forever blues (Kresh 273084, Pythas 273089, Anacondra 273088, Skum 273137);
-- each single-use boss reference is inlined together with the new item so the boss drops exactly one of its own set per kill,
-- and Mutanus gets his Hypnotic Crystal at the Wowhead rate (449 of 3,210 kills).
-- rollback: re-insert the reference pointers (3653,1,30122),(3670,1,30125),(3671,1,30126),(3674,1,30128) each with
--   Chance=100,QuestRequired=0,GroupId=0,MinCount=1,MaxCount=1,Comment='VMaNGOS 1.12', then DELETE the rows inserted below.
DELETE FROM `creature_loot_template` WHERE `Entry` IN (3653,3670,3671,3674) AND `ItemType` = 1 AND `Item` IN (30122,30125,30126,30128);
DELETE FROM `creature_loot_template` WHERE `Entry` IN (3653,3670,3671,3674) AND `ItemType` = 0 AND `GroupId` = 1
  AND `Item` IN (6447,13245,273084,6472,6473,273089,5404,6446,10412,273088,6448,6449,273137);
DELETE FROM `creature_loot_template` WHERE `Entry` = 3654 AND `ItemType` = 0 AND `Item` = 209838;
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(3653,0,6447,0,0,1,1,1,1,'Kresh - Worn Turtle Shell Shield'),
(3653,0,13245,0,0,1,1,1,1,'Kresh - Kresh\'s Back'),
(3653,0,273084,0,0,1,1,1,1,'Kresh - Cloak of Hermitic Bliss'),
(3670,0,6473,0,0,1,1,1,1,'Lord Pythas - Armor of the Fang'),
(3670,0,6472,0,0,1,1,1,1,'Lord Pythas - Stinging Viper'),
(3670,0,273089,0,0,1,1,1,1,'Lord Pythas - Slither Cord'),
(3671,0,5404,0,0,1,1,1,1,'Lady Anacondra - Serpent\'s Shoulders'),
(3671,0,6446,0,0,1,1,1,1,'Lady Anacondra - Snakeskin Bag'),
(3671,0,10412,0,0,1,1,1,1,'Lady Anacondra - Belt of the Fang'),
(3671,0,273088,0,0,1,1,1,1,'Lady Anacondra - Snake Eye Kaleidoscope'),
(3674,0,6449,0,0,1,1,1,1,'Skum - Glowing Lizardscale Cloak'),
(3674,0,6448,0,0,1,1,1,1,'Skum - Tail Spike'),
(3674,0,273137,0,0,1,1,1,1,'Skum - Skum\'s Bucket'),
(3654,0,209838,14.0,0,1,0,1,1,'Mutanus - Hypnotic Crystal (wowhead: 449 of 3,210 kills)');