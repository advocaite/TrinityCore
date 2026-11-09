-- Classic 1.60: Midsummer Celebrant (43 spawns) had the retail faction template 1734, which the 70532 client does not have
-- (FactionTemplate.db2), so the server treated it as no faction. VMaNGOS 1.12: 35 (friendly to everyone). Safe to run again.
UPDATE `creature_template` SET `faction` = 35 WHERE `entry` = 16781 AND `faction` = 1734;
