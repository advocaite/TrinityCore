-- Flintfire's Shipment (672508) had no loot id (Data1 = 0): "There is no loot". Per Wowhead Forever it always contains
-- one Flintfire's Shipment item (280413), objective of quest 98321 (collect 8).
START TRANSACTION;

UPDATE `gameobject_template` SET `Data1` = 672508, `Data8` = 98321 WHERE `entry` = 672508;

DELETE FROM `gameobject_loot_template` WHERE `Entry` = 672508;
INSERT INTO `gameobject_loot_template` (`Entry`, `ItemType`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(672508, 0, 280413, 100, 1, 1, 0, 1, 1, 'Flintfire''s Shipment');
COMMIT;
