-- Migration: 0034_sfk_route_floor
-- Date:    2026-10-28
-- Purpose: Shadowfang Keep's dungeon route (playerbot_dungeon_routes, map 33) runs from the courtyard to the stair at
--          (-222,2268) along a walkway at z 76.4-77.6, but four of its crumbs (seq 27-30) sat on the floor 2.5 y below it
--          (z 74.0). That floor joins the walkway only back through the courtyard: a tank on the walkway asked for a 268 y
--          detour to reach the crumb under its feet, and another 296 y one to come back up. The crumbs move onto the walkway
--          (heights read from the navmesh at each spot; seq 27 moves 1.7 y to where the walkway begins). Every leg from
--          seq 26 to seq 31 is then a direct walk of 2-8 polygons.
-- Reverts: yes (set the four rows back to their old values, listed below).
--          27 (-182.847, 2241.34, 74.043)  28 (-192.209, 2248.51, 74.043)  29 (-202.142, 2255.24, 74.043)
--          30 (-212.075, 2261.98, 74.035)

-- @target: shared

UPDATE {SHARED}.playerbot_dungeon_routes SET position_x = -183.6, position_y = 2242.9, position_z = 76.744
    WHERE map_id = 33 AND difficulty = 0 AND seq = 27;
UPDATE {SHARED}.playerbot_dungeon_routes SET position_z = 76.628
    WHERE map_id = 33 AND difficulty = 0 AND seq = 28;
UPDATE {SHARED}.playerbot_dungeon_routes SET position_z = 76.628
    WHERE map_id = 33 AND difficulty = 0 AND seq = 29;
UPDATE {SHARED}.playerbot_dungeon_routes SET position_z = 77.552
    WHERE map_id = 33 AND difficulty = 0 AND seq = 30;

INSERT INTO {SHARED}.playerbot_v2_schema_version (version, sha256) VALUES
    (34, REPEAT('0', 64))
ON DUPLICATE KEY UPDATE applied_at = CURRENT_TIMESTAMP;
