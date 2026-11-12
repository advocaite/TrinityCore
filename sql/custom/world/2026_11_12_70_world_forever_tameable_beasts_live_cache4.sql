-- Hunter pets (0.1.36), follow-up to 2026_11_11_70: what a fourth player cache (Russian client 70888, cache-sniff-1112,
-- report-only-1112.json) says about taming. Blizzard's live values only; nothing here touches pets players already have.
-- CreatureTemplate::IsTameable needs a beast, a pet family and TypeFlags bit 1. Later than 2026_11_09_12 (open world rule,
-- INFERRED), so these live answers win over it. Safe to run more than once.

-- 1. Not tameable on live (ours were).
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` & ~1 WHERE `Entry` = 819;   -- Servant of Ilgalar (cache)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` & ~1 WHERE `Entry` = 2462;  -- Flesh Eating Worm (cache)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` & ~1 WHERE `Entry` = 5755;  -- Deviate Viper (cache)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` & ~1 WHERE `Entry` = 6215;  -- Chomper (cache)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` & ~1 WHERE `Entry` = 14865; -- Felinni (cache; family 2 stays, from 2026_11_11_70)

-- 2. No pet family on live (ours 42 and 130). Flesh Eating Worm's live creature type 10 is left to the data files on purpose.
UPDATE `creature_template` SET `family` = 0 WHERE `entry` = 2462; -- Flesh Eating Worm, ours 42 (cache)
UPDATE `creature_template` SET `family` = 0 WHERE `entry` = 6215; -- Chomper, ours 130 (cache)

-- 3. Tameable on live with family 11 (raptor); ours had neither.
UPDATE `creature_template` SET `family` = 11 WHERE `entry` = 271336; -- Carnage (cache)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` | 1 WHERE `Entry` = 271336; -- Carnage (cache)

-- 4. Warlock minion family live sends (ours 0): Succubus Minion 17, like the Voidwalker and Imp Minions in 2026_11_11_70.
UPDATE `creature_template` SET `family` = 17 WHERE `entry` = 10928; -- Succubus Minion (cache)
