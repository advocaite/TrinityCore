-- Migration: 0039_owner_tactics
-- Date:    2026-11-10
-- Purpose: The player's own plan per raid boss, given in plain words ("tactics Lucifron: adds first, no aoe, tank here";
--          Bot/Raid/BossTactics.h). The raid brain applies it every time that boss is in the fight. Per owner account and boss,
--          kept across relog and restart.
--
--          Columns:
--            owner_account_id   the player's account
--            boss_entry         the boss's creature entry
--            tactics            the stored plan (SerializeTactics: "kill=adds;aoe=0;cc=1;spot=409,1012.5,-480.2,-180.0")
-- Reverts: yes (DROP TABLE playerbot_v2_owner_tactics).

CREATE TABLE IF NOT EXISTS playerbot_v2_owner_tactics (
    owner_account_id INT UNSIGNED NOT NULL,
    boss_entry       INT UNSIGNED NOT NULL,
    tactics          VARCHAR(255) NOT NULL DEFAULT '',
    PRIMARY KEY (owner_account_id, boss_entry)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO playerbot_v2_schema_version (version, sha256) VALUES
    (39, REPEAT('0', 64))
ON DUPLICATE KEY UPDATE applied_at = CURRENT_TIMESTAMP;
