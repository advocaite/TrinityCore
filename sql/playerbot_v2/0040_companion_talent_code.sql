-- Migration: 0040_companion_talent_code
-- Date:    2026-11-12
-- Purpose: A player can give a companion a talent build of their own, made in the Wowhead Forever talent calculator: "change talents
--          <link or code>" (Api/TalentCode.h). The bot learns that build instead of ours and puts each new level's point down it.
--          The code is stored with the companion so a relog or restart keeps it; "talents default" (or a setspec) clears it.
--
--          Columns:
--            companion_talents   '' = none, otherwise the talent code, digits per tab joined with '-' ("--05003201")
-- Reverts: yes (DROP COLUMN companion_talents).

ALTER TABLE playerbot_v2_character
    ADD COLUMN companion_talents VARCHAR(64) NOT NULL DEFAULT '';

INSERT INTO playerbot_v2_schema_version (version, sha256) VALUES
    (40, REPEAT('0', 64))
ON DUPLICATE KEY UPDATE applied_at = CURRENT_TIMESTAMP;
