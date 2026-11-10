-- Riverglades towns get their side back. 2026_11_09_43 (and the rows before it) left these NPCs on faction 35 (friendly to everyone),
-- so nobody could fight them, but Farholde's sentries also never defended the town, the Alliance gryphon master Gretchen Mayberry
-- served the Horde too. Sides follow the town, the way Blizzard does it in the old zones: Farholde Keep, Meadowsbrook, Wheeler's Grange and Turner's Logging Camp are human (their quests are
-- Alliance-only in the live data), Rog'mar is orc (wind rider master, grunts already Orgrimmar), Powderfuse Port is goblin.
-- ESTIMATED until a sniff has the real faction ids: Alliance guards 11 / folk 12 (Stormwind), Horde folk 29 (Orgrimmar),
-- Powderfuse Bruisers 121 and folk 120 (Booty Bay: everyone may trade, the bruisers fight whoever starts trouble).
-- Left on 35 on purpose: Fuzzle and Sleebo Fizzlespout (goblin quests open to both), Farmer Elbrim, the Lost Horse, the Bolder'ok ogres.
-- Only rows still on 35 change, so a later real value is never overwritten. Safe to run again.
UPDATE `creature_template` SET `faction` = 11 WHERE `faction` = 35 AND `entry` IN (255774, 256840, 257079, 257080, 257926, 259051, 259352, 260045, 260047, 260048, 260049, 260050, 260051, 260052, 260054, 260055, 260056, 260060, 260061, 260062, 260066, 260067, 260068, 260075, 260079, 260494, 260860, 269689, 269692, 269693, 271746);
UPDATE `creature_template` SET `faction` = 12 WHERE `faction` = 35 AND `entry` IN (256383, 256433, 256434, 256438, 257087, 257558, 257591, 257595, 258317, 258935, 258963, 259049, 259050, 259250, 259287, 259353, 259390, 259859, 259860, 259861, 259863, 260080, 262031, 263089, 263611, 263613, 263614, 268663, 269682, 269683, 269684, 269685, 269686, 269687, 269688, 269690, 269691, 269695, 269697, 269698, 269699, 270310, 270545, 270570, 270571, 270572, 270667, 271005, 271006, 271007, 271736, 271742, 272435, 272437, 272641, 272646, 275980);
UPDATE `creature_template` SET `faction` = 121 WHERE `faction` = 35 AND `entry` = 255890;
UPDATE `creature_template` SET `faction` = 120 WHERE `faction` = 35 AND `entry` IN (255891, 255894, 255895, 255897, 257292, 257294, 259766, 259961, 259962, 260231, 260359, 260471, 260487, 260525, 260540, 260543, 260558, 260559, 260560, 260561, 260562, 260563, 260564, 260565, 260566, 260568, 260570, 262284, 263120, 263326, 263384);
UPDATE `creature_template` SET `faction` = 29 WHERE `faction` = 35 AND `entry` IN (256418, 256658, 256675, 256710, 258031, 263071, 263190);
