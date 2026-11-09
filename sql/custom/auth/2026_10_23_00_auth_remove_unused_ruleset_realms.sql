-- The login screen lists only the realm that is played: remove the unused ruleset realms 71 (Forever PvP), 72 (Forever RP) and
-- 73 (Forever Hardcore) added by 2026_10_01_01_auth_classic_ruleset_realms.sql. No worldserver serves them, so the client showed
-- them as extra (offline) realms. A realmlist flag cannot hide a realm: the realm list sends every row, offline ones greyed out.
-- Kept: realm 70 (the Forever realm) and every other realm, including test realm 74. Safe to run again.
DELETE FROM `realmcharacters` WHERE `realmid` IN (71,72,73);
DELETE FROM `realmlist` WHERE `id` IN (71,72,73);
