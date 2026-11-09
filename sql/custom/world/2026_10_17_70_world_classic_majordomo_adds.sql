-- Molten Core, Majordomo Executus' eight adds: no Dark Mending on the Flamewaker Healer (11663; the 2026_10_16_00 row was our own, the vanilla spell list gives
-- the Healers no heal) and 20 percent less health and damage on the Healer (11663) and the Flamewaker Elite (11664).
-- creature_classic_stats before: 11663 health 40, damage 12; 11664 health 40, damage 16.85. After: 11663 32 / 9.6; 11664 32 / 13.48.
DELETE FROM world.smart_scripts WHERE source_type = 0 AND entryorguid = 11663 AND id = 2 AND action_type = 11 AND action_param1 = 19775;
UPDATE creature_classic_stats SET health_multiplier = 32, damage_multiplier = 9.6 WHERE entry = 11663;
UPDATE creature_classic_stats SET health_multiplier = 32, damage_multiplier = 13.48 WHERE entry = 11664;
