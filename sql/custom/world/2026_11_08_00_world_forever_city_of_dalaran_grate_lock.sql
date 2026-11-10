-- City of Dalaran: the Sewer Grate (656283) opened for nobody, key or not (the owner, 2026-11-07, Dalaran Sewer Key 277507 in his bags).
-- The grate is a door with lock 4468 (Data1). The game client checks that lock itself and, when the key it wants is not the one we
-- hand out, never tells the server the grate was clicked, so the script's key check never ran. The lock is cleared: every click
-- reaches the server, and the script (classic_go_cod_sewer_grate) checks the key, the group and ghosts, and says why it stays shut.
-- Safe to run again.
UPDATE `gameobject_template` SET `Data1` = 0 WHERE `entry` = 656283;
