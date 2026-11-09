-- Blackrock Spire: remove the retail Instance Portal inside the instance (guid 400651, map 229, near 63, -325, 54).
-- In retail it splits Lower and Upper Blackrock Spire into two maps; here both are one map, so the starry wall led nowhere
-- and players walked through it (reported 2026-11-08). The portals outside the instances stay.
DELETE FROM `gameobject` WHERE `guid`=400651 AND `map`=229;
DELETE FROM `gameobject_addon` WHERE `guid`=400651;
