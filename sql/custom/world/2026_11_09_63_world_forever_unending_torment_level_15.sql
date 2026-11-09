-- Blizzard hotfix 6 November 2026: "The Abomination's Head from The Baron (Ruins of Lordaeron) offers 'Unending Torment' to
-- Horde players at level 15, down from 16."
-- Ours had minimum level 16 on all five parts of the Unending Torment chain (97288-97292, from the beta sniff in
-- 2026_10_06_03_world_forever_quest_levels_sniff.sql). All five go to 15, so a level 15 player who starts it from the head can
-- also take the next parts. Quest level stays 21. Safe to run again.
UPDATE `quest_classic_level` SET `MinLevel`=15 WHERE `ID` IN (97288,97289,97290,97291,97292) AND `MinLevel`=16;
