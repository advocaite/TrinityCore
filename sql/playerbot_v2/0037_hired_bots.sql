-- Migration: 0037_hired_bots
-- Date:    2026-10-29
-- Purpose: Hired bots (Playerbot.Hire.*, Fleet/HireMgr). In hired mode a player's companions are hired from a mercenary NPC for
--          gold instead of being made out of nothing; each hired bot keeps its grade, what was paid for it and how long its
--          upkeep is paid. A player chooses hired bots for their account when the server leaves it to them (Hire.Mode = 2).
--
--          Columns (playerbot_v2_character):
--            hire_grade        0 = not hired (made before hired mode, or by a GM), 1 = Sellsword, 2 = Veteran, 3 = Legend
--            hire_price        copper paid for the bot; its daily upkeep is Playerbot.Hire.UpkeepPct of this
--            hire_paid_until   unix seconds; the upkeep is due again from then, the next time the bot is called
--          Table playerbot_v2_hire_pref: account_id, hired_only (1 = the account chose hired bots).
-- Reverts: yes (DROP COLUMN hire_grade, hire_price, hire_paid_until; DROP TABLE playerbot_v2_hire_pref).

ALTER TABLE playerbot_v2_character
    ADD COLUMN hire_grade TINYINT UNSIGNED NOT NULL DEFAULT 0,
    ADD COLUMN hire_price BIGINT UNSIGNED NOT NULL DEFAULT 0,
    ADD COLUMN hire_paid_until BIGINT UNSIGNED NOT NULL DEFAULT 0;

CREATE TABLE IF NOT EXISTS playerbot_v2_hire_pref (
    account_id INT UNSIGNED     NOT NULL,
    hired_only TINYINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (account_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO playerbot_v2_schema_version (version, sha256) VALUES
    (37, REPEAT('0', 64))
ON DUPLICATE KEY UPDATE applied_at = CURRENT_TIMESTAMP;
