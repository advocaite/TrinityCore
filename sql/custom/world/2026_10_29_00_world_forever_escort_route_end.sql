-- Escorts that walk their whole route but never give the escort credit (Discord, Beastmade 2026-10-29: Pal'juh, Lost in the
-- Shadows 99123). The other Forever server escorts (2026_10_26_38) give the credit on SMART_EVENT_WAYPOINT_ENDED with point 0, but in this
-- core point 0 means "only when the route ends on node 0" ("any node" is 0xFFFFFFFF), and the routes run from node 1, so the
-- event never fired. The route start's quest (WP_START action_param4) gives nothing either, since the route is started on the
-- escort itself, not on the player, and the quests want the escort's kill credit. Now the credit (and for Seeking Refuge
-- 99144 its completion event, 2026_10_28_70) comes when the route ends on any node.
--   Lumina Windsinger 248755 The Windshaper's Wrath 96204, Perith Stormhoof 272203 The Longwalkers 98430, Pal'juh 275811 Lost in the
--   Shadows 99123, Bareth Dawnstone 275954 Seeking Refuge 99144.
-- Safe to run again.
UPDATE `smart_scripts` SET `event_param1`=4294967295
WHERE `source_type`=0 AND `event_type`=58 AND `id`=12 AND `entryorguid` IN (248755, 272203, 275811, 275954);
