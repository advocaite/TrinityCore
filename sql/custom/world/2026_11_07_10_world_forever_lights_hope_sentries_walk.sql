-- Light's Hope Chapel: the Argent Sentries (16378) swam in the air. The retail difficulty row they keep from TDB has
-- CREATURE_STATIC_FLAG_FLOATING (0x20000000), which turns gravity off and plays the hover animation (lying flat while they
-- walk, legs dangling while they stand). They are plain guards on foot, as in vanilla, so the flag goes.
-- Checked: of the vanilla creatures that are spawned and carry this flag, the sentry is the only humanoid; the rest are birds,
-- bats, wisps, dragons, gargoyles and invisible triggers, which are right to float.
UPDATE `creature_template_difficulty` SET `StaticFlags1` = `StaticFlags1` & ~0x20000000 WHERE `Entry` = 16378;
