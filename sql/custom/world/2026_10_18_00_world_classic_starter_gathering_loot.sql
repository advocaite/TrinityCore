-- Classic 1.60: loot for the Forever starting-zone gathering nodes the sniffs never saw opened, so they gave "There is no loot."
-- Wilted Peacebloom 656160 (and its Tauren Cultivation twin 657454): 1 Peacebloom, 100%. Poor Copper Vein 562111: 1 Copper Ore
-- and 1 Rough Stone, both 100% (Wowhead Forever, like Stunted Silverleaf 656161's sniffed 1 Silverleaf). Safe to run again.
DELETE FROM `gameobject_loot_template` WHERE `Entry` IN (141507,134420);
INSERT INTO `gameobject_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(141507,0,2447,100,0,1,0,1,1,'Wilted Peacebloom - Peacebloom'),
(134420,0,2770,100,0,1,0,1,1,'Poor Copper Vein - Copper Ore'),
(134420,0,2835,100,0,1,0,1,1,'Poor Copper Vein - Rough Stone');
