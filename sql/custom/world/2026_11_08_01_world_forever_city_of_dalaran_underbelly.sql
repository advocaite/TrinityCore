-- City of Dalaran, Underbelly (the owner, 2026-11-08, in game):
-- 1. Fallen Warrior (255069) had faction 35 (friendly) and stood idle next to the hostile Fallen Mages and Kirin Tor
--    Necromancers. It gets the Fallen Mage's faction 21 (INFERRED: same undead group, no live data).
-- 2. Angry Tome (254856) sat in the floor: a flying book with no hover. It hovers now.
-- 3. The round door at the top of the sewer hill (665036) had lock 77, which the game client checks itself, so it only said
--    "The door is locked". The lock is cleared; the instance script opens it once Atrexis and Lyn are dead and otherwise says
--    why it stays shut.
-- Safe to run again.
UPDATE `creature_template` SET `faction` = 21 WHERE `entry` = 255069;

DELETE FROM `creature_template_movement` WHERE `CreatureId` = 254856;
INSERT INTO `creature_template_movement` (`CreatureId`,`HoverInitiallyEnabled`,`Chase`,`Random`,`InteractionPauseTimer`) VALUES
(254856,1,NULL,NULL,NULL);

UPDATE `gameobject_template` SET `Data1` = 0, `ScriptName` = 'classic_go_cod_city_door' WHERE `entry` = 665036;
