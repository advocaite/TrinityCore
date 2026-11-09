-- Service NPCs that had the critter faction 188 (ambient: anyone may attack them), so world bots looking for something to
-- fight farmed the Azeroth Commerce Authority camp in Redridge (Eyelash, 11-02). These templates were built without a sniff
-- (VerifiedBuild 0). Vendors, trainers and quest givers are friendly to both factions (35); Gezzy Gunkgear is the Booty Bay
-- quartermaster and gets the Booty Bay faction (120) like the other Booty Bay NPCs. Safe to run again.
UPDATE `creature_template` SET `faction` = 35 WHERE `faction` = 188 AND `entry` IN
(256389, 256390, 256391, 256729, 256730, 256731, 256732, 256733, 256734, 256735, 256736, 256742, -- Azeroth Commerce Authority
 263644);                                                                                          -- Pherry Leftee, battlemaster
UPDATE `creature_template` SET `faction` = 120 WHERE `faction` = 188 AND `entry` = 265575;         -- Gezzy Gunkgear, Booty Bay
