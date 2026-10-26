-- Shaman Shrine (100028, 101749, 101750, 101751) is spell focus 63 ("Santuario del chamán", SpellFocusObject.db2).
-- "Mirada de sapta" (8202, 8898, 8899, 8900, 9735) requires that focus to drink the Earth Sapta, but the shrines were
-- goobers (type 10), so the focus check never matched and the cast failed with "Requires Shaman Shrine".
-- Type 8 uses data0 = focus id and data1 = radius in yards; data3 is serverOnly for focus objects. Radius 20 matches
-- Shrine of Remulos (177788, focus 1043).
UPDATE `gameobject_template` SET `type` = 8, `data0` = 63, `data1` = 20, `data3` = 0 WHERE `entry` IN (100028, 101749, 101750, 101751);

-- Dun Morogh (map 0, zone 1, area 1): the stone where the dwarf shaman drinks the Earth Sapta had no object on the server,
-- so the focus check found nothing. Position taken in game with .gps standing next to it.
REPLACE INTO `gameobject` VALUES (30632463,100028,0,1,1,'0',0,0,0,-1,-5901.607910,585.681946,502.214966,0,0,0,0,1,900,100,1,'',NULL,0);
