-- Creatures of our added spawn block (46000000+, 2026_10_30_10) that stand on a spawn of the same NPC the world already has
-- (same id, under 5 y apart). Two of one mob on one spot: when one dies the other is still there, so it looks like the corpse
-- got up again (Dun Morogh 51,43, Minor Ice Elemental, Discord 2026-11-09). Also two Grenhild Darktalon and two Avatar of
-- Saeyleenan standing in each other. The world's own spawn stays.
DELETE FROM `creature_addon` WHERE `guid` IN (46000001,46000011,46000012,46000013,46000014,46000015,46000021,46000022,46000023,46000024,
    46000026,46000027,46000029,46000031,46000032,46000033,46000034,46000035,46000041);
DELETE FROM `creature` WHERE `guid` IN (46000001,46000011,46000012,46000013,46000014,46000015,46000021,46000022,46000023,46000024,
    46000026,46000027,46000029,46000031,46000032,46000033,46000034,46000035,46000041);
