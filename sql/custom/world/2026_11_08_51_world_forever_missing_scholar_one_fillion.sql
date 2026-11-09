-- The Missing Scholar (92849, Zephras Isle): two Fillion Flamebreeze stood at the cave (kitaecxd, 2026-11-08). Fillion "being
-- carried" (253281, rides the player as a vehicle passenger, aura 46598) was in the sniff as four fixed spawns, one 1.7 yd from the
-- injured Fillion (253002) and three on the path out. He is never a standing NPC: the quest credit for 253281 comes from the
-- carry spell or from the injured Fillion's own script (2026_11_01_45), and the safe Fillion (253284) despawns a carried one
-- nearby. So the fixed spawns go; the injured (253002) and the safe (253284) Fillion stay.
-- Check in game: .go xyz 2532.5 1412.1 649.7 2991 shows one Fillion; the quest still finishes at the safe spot. Safe to run again.
DELETE FROM `creature` WHERE `id` = 253281 AND `map` = 2991 AND `guid` IN (21101232, 21101233, 21101234, 43000304);
