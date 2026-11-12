-- Gathering and lockpicking: the skill colour on the tooltip of herbs, veins and locked chests.
-- The client colours the profession line of a node's tooltip (red, orange, yellow, green, gray) from the object's trivial skill range,
-- the gameobject_template fields Data23 (trivialSkillLow: yellow from here) and Data24 (trivialSkillHigh: gray from here) of a chest.
-- Our templates came without them, so the client saw 0 / 0 and (likely; not tested in game) drew every herb gray, while the server (Player::UpdateGatherSkill,
-- which falls back to the classic +25 / +50 / +100 ranks when they are 0) still gave skill points (Discord, Gremloc 2026-11-11:
-- "the herbalism text appears gray even though it is adding skill points").
-- The values are the official server's, from its query replies in the players' client caches (gameobjectcache.wdb, enUS / ruRU / deDE,
-- builds 69933 to 70862; every object read the same in every cache). With them the server's skill-up chances follow the same ranges.
-- Only type 3 (chest) rows are changed. Safe to run again.
UPDATE `gameobject_template` SET `Data23` = 270, `Data24` = 345 WHERE `entry` = 324 AND `type` = 3; -- Small Thorium Vein
UPDATE `gameobject_template` SET `Data23` = 90, `Data24` = 165 WHERE `entry` = 1610 AND `type` = 3; -- Incendicite Mineral Vein
UPDATE `gameobject_template` SET `Data23` = 25, `Data24` = 100 WHERE `entry` = 1617 AND `type` = 3; -- Silverleaf
UPDATE `gameobject_template` SET `Data23` = 25, `Data24` = 100 WHERE `entry` = 1618 AND `type` = 3; -- Peacebloom
UPDATE `gameobject_template` SET `Data23` = 40, `Data24` = 115 WHERE `entry` = 1619 AND `type` = 3; -- Earthroot
UPDATE `gameobject_template` SET `Data23` = 75, `Data24` = 150 WHERE `entry` = 1620 AND `type` = 3; -- Mageroyal
UPDATE `gameobject_template` SET `Data23` = 95, `Data24` = 170 WHERE `entry` = 1621 AND `type` = 3; -- Briarthorn
UPDATE `gameobject_template` SET `Data23` = 125, `Data24` = 200 WHERE `entry` = 1622 AND `type` = 3; -- Bruiseweed
UPDATE `gameobject_template` SET `Data23` = 140, `Data24` = 215 WHERE `entry` = 1623 AND `type` = 3; -- Wild Steelbloom
UPDATE `gameobject_template` SET `Data23` = 150, `Data24` = 225 WHERE `entry` = 1624 AND `type` = 3; -- Kingsblood
UPDATE `gameobject_template` SET `Data23` = 145, `Data24` = 220 WHERE `entry` = 1628 AND `type` = 3; -- Grave Moss
UPDATE `gameobject_template` SET `Data23` = 90, `Data24` = 165 WHERE `entry` = 1667 AND `type` = 3; -- Incendicite Mineral Vein
UPDATE `gameobject_template` SET `Data23` = 25, `Data24` = 100 WHERE `entry` = 1731 AND `type` = 3; -- Copper Vein
UPDATE `gameobject_template` SET `Data23` = 90, `Data24` = 165 WHERE `entry` = 1732 AND `type` = 3; -- Tin Vein
UPDATE `gameobject_template` SET `Data23` = 100, `Data24` = 175 WHERE `entry` = 1733 AND `type` = 3; -- Silver Vein
UPDATE `gameobject_template` SET `Data23` = 180, `Data24` = 255 WHERE `entry` = 1734 AND `type` = 3; -- Gold Vein
UPDATE `gameobject_template` SET `Data23` = 150, `Data24` = 225 WHERE `entry` = 1735 AND `type` = 3; -- Iron Deposit
UPDATE `gameobject_template` SET `Data23` = 200, `Data24` = 275 WHERE `entry` = 2040 AND `type` = 3; -- Mithril Deposit
UPDATE `gameobject_template` SET `Data23` = 175, `Data24` = 250 WHERE `entry` = 2041 AND `type` = 3; -- Liferoot
UPDATE `gameobject_template` SET `Data23` = 185, `Data24` = 260 WHERE `entry` = 2042 AND `type` = 3; -- Fadeleaf
UPDATE `gameobject_template` SET `Data23` = 210, `Data24` = 285 WHERE `entry` = 2043 AND `type` = 3; -- Khadgar's Whisker
UPDATE `gameobject_template` SET `Data23` = 110, `Data24` = 185 WHERE `entry` = 2045 AND `type` = 3; -- Stranglekelp
UPDATE `gameobject_template` SET `Data23` = 195, `Data24` = 270 WHERE `entry` = 2046 AND `type` = 3; -- Goldthorn
UPDATE `gameobject_template` SET `Data23` = 255, `Data24` = 330 WHERE `entry` = 2047 AND `type` = 3; -- Truesilver Deposit
UPDATE `gameobject_template` SET `Data23` = 90, `Data24` = 165 WHERE `entry` = 2054 AND `type` = 3; -- Incendicite Mineral Vein (no enUS cache name)
UPDATE `gameobject_template` SET `Data23` = 25, `Data24` = 100 WHERE `entry` = 2055 AND `type` = 3; -- Copper Vein (no enUS cache name)
UPDATE `gameobject_template` SET `Data23` = 230, `Data24` = 305 WHERE `entry` = 2866 AND `type` = 3; -- Firebloom
UPDATE `gameobject_template` SET `Data23` = 30, `Data24` = 105 WHERE `entry` = 3714 AND `type` = 3; -- Alliance Strongbox
UPDATE `gameobject_template` SET `Data23` = 25, `Data24` = 100 WHERE `entry` = 3724 AND `type` = 3; -- Peacebloom
UPDATE `gameobject_template` SET `Data23` = 25, `Data24` = 100 WHERE `entry` = 3725 AND `type` = 3; -- Silverleaf (no enUS cache name)
UPDATE `gameobject_template` SET `Data23` = 40, `Data24` = 115 WHERE `entry` = 3726 AND `type` = 3; -- Earthroot
UPDATE `gameobject_template` SET `Data23` = 75, `Data24` = 150 WHERE `entry` = 3727 AND `type` = 3; -- Mageroyal
UPDATE `gameobject_template` SET `Data23` = 95, `Data24` = 170 WHERE `entry` = 3729 AND `type` = 3; -- Briarthorn
UPDATE `gameobject_template` SET `Data23` = 125, `Data24` = 200 WHERE `entry` = 3730 AND `type` = 3; -- Bruiseweed
UPDATE `gameobject_template` SET `Data23` = 25, `Data24` = 100 WHERE `entry` = 3763 AND `type` = 3; -- Copper Vein
UPDATE `gameobject_template` SET `Data23` = 90, `Data24` = 165 WHERE `entry` = 3764 AND `type` = 3; -- Tin Vein
UPDATE `gameobject_template` SET `Data23` = 90, `Data24` = 165 WHERE `entry` = 103711 AND `type` = 3; -- Tin Vein
UPDATE `gameobject_template` SET `Data23` = 25, `Data24` = 100 WHERE `entry` = 103713 AND `type` = 3; -- Copper Vein
UPDATE `gameobject_template` SET `Data23` = 55, `Data24` = 130 WHERE `entry` = 121264 AND `type` = 3; -- Lucius's Lockbox
UPDATE `gameobject_template` SET `Data23` = 100, `Data24` = 175 WHERE `entry` = 123214 AND `type` = 3; -- Duskwood Chest
UPDATE `gameobject_template` SET `Data23` = 200, `Data24` = 275 WHERE `entry` = 123310 AND `type` = 3; -- Ooze Covered Mithril Deposit
UPDATE `gameobject_template` SET `Data23` = 30, `Data24` = 105 WHERE `entry` = 123330 AND `type` = 3; -- Buccaneer's Strongbox
UPDATE `gameobject_template` SET `Data23` = 30, `Data24` = 105 WHERE `entry` = 123331 AND `type` = 3; -- Buccaneer's Strongbox
UPDATE `gameobject_template` SET `Data23` = 235, `Data24` = 310 WHERE `entry` = 142140 AND `type` = 3; -- Purple Lotus
UPDATE `gameobject_template` SET `Data23` = 255, `Data24` = 330 WHERE `entry` = 142142 AND `type` = 3; -- Sungrass
UPDATE `gameobject_template` SET `Data23` = 275, `Data24` = 350 WHERE `entry` = 142145 AND `type` = 3; -- Gromsblood
UPDATE `gameobject_template` SET `Data23` = 255, `Data24` = 330 WHERE `entry` = 165658 AND `type` = 3; -- Dark Iron Deposit
UPDATE `gameobject_template` SET `Data23` = 300, `Data24` = 375 WHERE `entry` = 175404 AND `type` = 3; -- Rich Thorium Vein
UPDATE `gameobject_template` SET `Data23` = 285, `Data24` = 360 WHERE `entry` = 176583 AND `type` = 3; -- Golden Sansam
UPDATE `gameobject_template` SET `Data23` = 290, `Data24` = 365 WHERE `entry` = 176584 AND `type` = 3; -- Dreamfoil
UPDATE `gameobject_template` SET `Data23` = 305, `Data24` = 380 WHERE `entry` = 176586 AND `type` = 3; -- Mountain Silversage
UPDATE `gameobject_template` SET `Data23` = 30, `Data24` = 105 WHERE `entry` = 178244 AND `type` = 3; -- Practice Lockbox
UPDATE `gameobject_template` SET `Data23` = 30, `Data24` = 105 WHERE `entry` = 178245 AND `type` = 3; -- Practice Lockbox
UPDATE `gameobject_template` SET `Data23` = 30, `Data24` = 105 WHERE `entry` = 178246 AND `type` = 3; -- Practice Lockbox
UPDATE `gameobject_template` SET `Data23` = 100, `Data24` = 175 WHERE `entry` = 179486 AND `type` = 3; -- Battered Footlocker
UPDATE `gameobject_template` SET `Data23` = 100, `Data24` = 175 WHERE `entry` = 179487 AND `type` = 3; -- Waterlogged Footlocker
UPDATE `gameobject_template` SET `Data23` = 140, `Data24` = 215 WHERE `entry` = 179488 AND `type` = 3; -- Battered Footlocker
UPDATE `gameobject_template` SET `Data23` = 140, `Data24` = 215 WHERE `entry` = 179489 AND `type` = 3; -- Waterlogged Footlocker
UPDATE `gameobject_template` SET `Data23` = 230, `Data24` = 305 WHERE `entry` = 179494 AND `type` = 3; -- Dented Footlocker
UPDATE `gameobject_template` SET `Data23` = 255, `Data24` = 330 WHERE `entry` = 179496 AND `type` = 3; -- Dented Footlocker
UPDATE `gameobject_template` SET `Data23` = 10, `Data24` = 20 WHERE `entry` = 562111 AND `type` = 3; -- Poor Copper Vein
UPDATE `gameobject_template` SET `Data23` = 10, `Data24` = 20 WHERE `entry` = 656160 AND `type` = 3; -- Wilted Peacebloom
UPDATE `gameobject_template` SET `Data23` = 10, `Data24` = 20 WHERE `entry` = 656161 AND `type` = 3; -- Stunted Silverleaf
