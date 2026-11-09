-- Rare Books 91777 (elite, Jasperlode Mine): players find Geomancer Yip but Mother Fang 471 is not there (Discord 11-09).
-- The quest needs 248004 from her (100% drop, 2026_11_01_13). Our Mother Fang is still the classic rare: Classification 4 and
-- one spawn (20080989, at the mine) with a respawn of one hour, so after one kill by a player or a bot nobody sees her for an
-- hour. On Forever she is an elite quest target, level 7-10 (Wowhead Forever: "Rank Elite instead of Rare"), like the other
-- named targets of elite quests. Respawn 5 minutes, as our other named quest targets. INFERRED: the respawn time is not in
-- any source we have.
UPDATE `creature_template` SET `Classification` = 1 WHERE `entry` = 471;
UPDATE `creature` SET `spawntimesecs` = 300 WHERE `id` = 471 AND `map` = 0;
