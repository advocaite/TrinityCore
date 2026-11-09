-- Quest NPCs standing in two places at once: the import kept a second spawn of the same NPC away from her post (Discord reports
-- 2026-10-27, Aamelia Windfield; and both spawns of Sentinel Eralya Leafshadow). Each one's quests end at the spawn their quest
-- POIs point to, which stays; the other goes.
--   Aamelia Windfield 252800 (Zephras, sniff 2026_10_03_00): every quest of hers ends at 1905-1918, 1633-1637, the spawn by the
--   house (21101143); the second one stood 120 yards away at the archway (21101144).
--   Sentinel Eralya Leafshadow 275683 (the other Forever server port 2026_10_26_32): Easing Suffering 99073 and The Lost Runner 99046 end at
--   10582, 1910 (27568300); the second one stood 17 yards away (27568301).
DELETE FROM `creature` WHERE `guid`=21101144 AND `id`=252800;
DELETE FROM `creature` WHERE `guid`=27568301 AND `id`=275683;
DELETE FROM `creature_addon` WHERE `guid` IN (21101144, 27568301);
