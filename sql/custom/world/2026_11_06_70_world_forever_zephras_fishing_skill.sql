-- Classic 1.60 (WoW Forever): fishing on Zephras Isle (zone 16593, Skyborne start, also Shen'dar) almost never caught a fish.
-- The vanilla catch rule (GameObject::Use, fishing bobber) is chance = skill - zone skill + 5. Zephras Isle had no row here,
-- so the zone skill was 0: a new fisher (skill 1) caught only 6% of casts, and skill only rises on a catch.
-- Give Zephras Isle the same base level as the other starting zones (Durotar 14, Elwynn 12, Teldrassil 141, ...: -70),
-- so skill 1 catches about 76% of casts and 25+ catches every cast. Sub-areas use the zone row. Safe to run again.
DELETE FROM `skill_fishing_base_level` WHERE `entry` = 16593;
INSERT INTO `skill_fishing_base_level` (`entry`, `skill`) VALUES
(16593, -70);
