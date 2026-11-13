-- Classic 1.60 (WoW Forever): vanilla durability of the two Naxxramas items added in hotfixes 2026_11_12_39 (The Face of Death
-- 23043: 120, The Hungering Cold 23577: 105, VMaNGOS item_template.max_durability). 2026_10_26_00 lists only items the client
-- already had. Safe to run again.
DELETE FROM `classic_item_durability` WHERE `entry` IN (23043,23577);
INSERT INTO `classic_item_durability` (`entry`, `max_durability`) VALUES
(23043,120),
(23577,105);
