-- Rusty Gadget (257215) starts quest 93173 "(UNUSED) The Rusty Gadget", which Blizzard marks unused and which is not on this
-- realm: right-clicking the item did nothing (Discord player report 2026-11-05, Zephris Isle). 2026_10_29_01 had made Wind Hollow
-- (251676) drop it for everyone. The item no longer drops from any loot table. Safe to run again.
DELETE FROM `creature_loot_template` WHERE `Item` = 257215;
DELETE FROM `gameobject_loot_template` WHERE `Item` = 257215;
DELETE FROM `reference_loot_template` WHERE `Item` = 257215;
DELETE FROM `item_loot_template` WHERE `Item` = 257215;
