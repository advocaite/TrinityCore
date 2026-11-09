-- Stondry Darkhammer (256730), Azeroth Commerce Authority blacksmith in Redridge: his plans cost Merchant's Favor (currency 3402),
-- not silver (Eyelash, 11-02). Prices from the Forever vendor list (62 items). The ItemExtendedCost rows are the client's own
-- (70707), each Merchant's Favor x N with no money: 11844 = 30, 11846 = 60, 11847 = 90, 11848 = 120, 11522 = 1000.
-- With an ExtendedCost set the server no longer charges the item's silver price.
-- Every one of the 62 prices is from the list; none is inferred. Safe to run again.
UPDATE `npc_vendor` SET `ExtendedCost` = 11844 WHERE `entry` = 256730 AND `type` = 1 AND `item` BETWEEN 251355 AND 251384;   -- boots, gloves, belts, shirts, leggings, helms: 30
UPDATE `npc_vendor` SET `ExtendedCost` = 11846 WHERE `entry` = 256730 AND `type` = 1 AND `item` BETWEEN 251418 AND 251422;   -- gauntlets: 60
UPDATE `npc_vendor` SET `ExtendedCost` = 11846 WHERE `entry` = 256730 AND `type` = 1 AND `item` BETWEEN 251428 AND 251432;   -- pauldrons: 60
UPDATE `npc_vendor` SET `ExtendedCost` = 11847 WHERE `entry` = 256730 AND `type` = 1 AND `item` BETWEEN 251413 AND 251417;   -- sabatons: 90
UPDATE `npc_vendor` SET `ExtendedCost` = 11847 WHERE `entry` = 256730 AND `type` = 1 AND `item` BETWEEN 251423 AND 251427;   -- waistguards: 90
UPDATE `npc_vendor` SET `ExtendedCost` = 11847 WHERE `entry` = 256730 AND `type` = 1 AND `item` BETWEEN 251433 AND 251437;   -- wristguards: 90
UPDATE `npc_vendor` SET `ExtendedCost` = 11847 WHERE `entry` = 256730 AND `type` = 1 AND `item` IN (251465, 251466);          -- Thorium Greatmace, Thorium Cestus: 90
UPDATE `npc_vendor` SET `ExtendedCost` = 11848 WHERE `entry` = 256730 AND `type` = 1 AND `item` IN (251447, 251448, 251449, 251471); -- Enriched Thorium set, Legionite Glaive: 120
UPDATE `npc_vendor` SET `ExtendedCost` = 11522 WHERE `entry` = 256730 AND `type` = 1 AND `item` = 271622;                     -- Blacksmithing Certification: 1000
