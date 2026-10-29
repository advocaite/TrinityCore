-- Classic 1.60: Emperor Dagran, Panzar, Bael'Gar and Ambassador Flamelash carried references whose items do not exist in
-- the Forever universe (Wowhead tooltip "Entity not found", zero drops observed across 10k+ kills each); the dead
-- pointers are removed. Ribbly's Wowhead-observed Plans: Tempered Dark Iron Plate (227901, 518 of 2,105 kills) is added
-- at its measured rate. All other BRD boss set items (Argelmach, Incendius, Gerstahn, Loregrain, Roccor, Angerforge,
-- Stilgiss, Fineous, Grebmar, Magmus, Plugger) are Wowhead-real but missing from this client's item data — rows stay inert.
-- rollback: re-insert (9019,1,30553),(9019,1,30554),(8923,1,30222),(9016,1,30226),(9156,1,30243) each with
--   Chance=100,QuestRequired=0,GroupId=0,MinCount=1,MaxCount=1,Comment='VMaNGOS 1.12';
--   DELETE FROM creature_loot_template WHERE Entry=9543 AND ItemType=0 AND Item=227901; then .reload creature_loot_template.
DELETE FROM `creature_loot_template` WHERE `Entry` = 9019 AND `ItemType` = 1 AND `Item` IN (30553,30554);
DELETE FROM `creature_loot_template` WHERE `Entry` IN (8923,9016,9156) AND `ItemType` = 1 AND `Item` IN (30222,30226,30243);
DELETE FROM `creature_loot_template` WHERE `Entry` = 9543 AND `ItemType` = 0 AND `Item` = 227901;
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(9543,0,227901,24.56,0,1,0,1,1,'Ribbly Screwspigot - Plans: Tempered Dark Iron Plate (wowhead: 518 of 2,105 kills)');