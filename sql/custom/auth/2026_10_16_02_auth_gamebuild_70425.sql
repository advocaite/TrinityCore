-- Realms 70-73 take the WoW Classic beta client 1.60.1.70425 (build_info row: 2026_10_16_00_auth_classic_beta_70425.sql).
-- A realm only accepts a client whose build equals realmlist.gamebuild. Safe to run again.
UPDATE `realmlist` SET `gamebuild`=70425 WHERE `id` IN (70,71,72,73);
