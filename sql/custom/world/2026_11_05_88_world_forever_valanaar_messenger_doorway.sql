-- High Order Messenger 273967 in Valanaar, Zephras Isle (Discord, alexia5746 2026-11-05): one stands still in a doorway and
-- blocks the way. Our sniffs have him walking between the house and the street (spawns 21101950 and 21101951 with their
-- paths); the Azeroth import added a third, standing copy, 43000395, on the end of that walk (2048.8, 933.5), 6 yd from our
-- sniffed spawn, just over the 5 yd it used to leave copies out. Remove that standing copy. Safe to run again.
DELETE FROM `creature` WHERE `guid`=43000395 AND `id`=273967;
