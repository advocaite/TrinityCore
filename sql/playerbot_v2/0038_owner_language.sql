-- Migration: 0038_owner_language
-- Date:    2026-10-28
-- Purpose: The language the bots answer a player in, chosen with `.bot language auto|en|ru`. Without a row the player's client
--          language is used (Russian for a ruRU client, English otherwise). Per owner account, kept across relog and restart.
--
--          Columns:
--            owner_account_id   the player's account (one row per account that chose)
--            language           1 = English, 2 = Russian (BotLanguage::Pref; "auto" deletes the row)
-- Reverts: yes (DROP TABLE playerbot_v2_owner_language).

CREATE TABLE IF NOT EXISTS playerbot_v2_owner_language (
    owner_account_id INT UNSIGNED     NOT NULL,
    language         TINYINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (owner_account_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO playerbot_v2_schema_version (version, sha256) VALUES
    (38, REPEAT('0', 64))
ON DUPLICATE KEY UPDATE applied_at = CURRENT_TIMESTAMP;
