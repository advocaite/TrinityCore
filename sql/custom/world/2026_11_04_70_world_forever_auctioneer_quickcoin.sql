-- Auctioneer Quickcoin (258878, Azeroth Commerce Authority camp, Redridge) could be attacked and opened no auction house: his
-- template came from the name-only NPC rebuild (neutral faction 7, gossip flag only). Same as the other Commerce Authority
-- auctioneer Zelena Favorbreeze (257037, official sniff 70124): faction 35 (friendly to all), auctioneer flag, unit_flags2 2048.
-- Safe to run again.
UPDATE `creature_template` SET `faction` = 35, `npcflag` = `npcflag` | 2097152, `unit_flags2` = `unit_flags2` | 2048 WHERE `entry` = 258878;
