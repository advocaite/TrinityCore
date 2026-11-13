-- Two Dun Morogh NPCs stood inside something (Discord bug reports, 2026-11). Heights measured with `.playerbot groundz` on the test
-- realm (steps 76 and 80, 2026-11-12).

-- Thordin Coppervein (6110001), hire broker in the Thunderbrew Distillery, Kharanos (voldemargreed: "stands inside the inn
-- counter"). His spot beside innkeeper Belm is the counter itself: probed from 401.5 the first surface there is the counter top
-- at 400.52. At -5598.50, -526.50 the first surface is the floor, 399.65 (the cellar 392.43 below it), the same floor the
-- merchants beside it stand on (Kreg Bilmn 1691 at 399.74). He stands there, facing Belm.
UPDATE `creature` SET `position_x` = -5598.50, `position_y` = -526.50, `position_z` = 399.65, `orientation` = 4.13
WHERE `guid` = 61100001 AND `id` = 6110001;

-- Mountaineer Coalbeard (276080) stood on exactly the same spot as Minor Ice Elemental 27600304 (-5321.75, -758.917), which
-- wanders 5 yards around it, so the elemental walked through him. He moves to one of his own Forever sightings (Questie traces,
-- 51.64, 42.85) that is 21 yards from every ice elemental; the ground there is 389.16.
UPDATE `creature` SET `position_x` = -5283.99, `position_y` = -741.19, `position_z` = 389.16
WHERE `guid` = 27608000 AND `id` = 276080;
