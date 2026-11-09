-- Classic 1.60: Alterac Valley Horde start. The old point (-1437.67,-610.089,51.16) is the far end of the vanilla Horde tunnel; the 70532
-- map 30 has no tunnel there (no model covers it, the terrain is at z 168.8 above it), so players started inside the hill and bots found no
-- navmesh. The new point is behind the Horde gate (180424 at -870.9,-558.6) in the cave model that holds the gate, on the navmesh (probe
-- snap z 70.235, path to Tower Point found), facing the gate. Safe to run again.
UPDATE `world_safe_locs` SET `LocX` = -890.0, `LocY` = -575.0, `LocZ` = 70.235, `Facing` = 41.8 WHERE `ID` = 610;
