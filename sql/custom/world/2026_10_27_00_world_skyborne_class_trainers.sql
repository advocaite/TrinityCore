-- Zephras Isle (Skyborne): the second-tier class trainers could not train. Seena Skybreaker and Corsan Earthrazer (warrior),
-- Anathamaas Aetherwind and Shenaan Spellwind (mage), Eltheen Nightbreeze and Miriaan Mistblade (rogue), Sessaria Skystride and
-- Aarnor Galestrike (shaman) had no trainer flag and no trainer; Lotheluum Starbreeze (druid) and Elayaa Easewind (hunter) had a
-- trainer but no flag; the starting rogue trainer Akeri Duskblade had the flag but no trainer. Wowhead Forever lists all of them as
-- class trainers and the other Forever server trains with them. Each gets the existing trainer whose spell list matches the other Forever server's (warrior 23,
-- rogue 26, shaman 10, starting rogue 25); the mage trainers get the main mage list 1, which is the other Forever server's minus the three Frost
-- Blast ranks (1326053-1326055, on no trainer of ours) plus the race-gated teleports and portals.
-- The menu has no "training" option of its own: Player::PrepareGossipMenu adds "Train me." for a trainer-flagged NPC with a trainer.

UPDATE `creature_template` SET `npcflag` = `npcflag` | 48 -- UNIT_NPC_FLAG_TRAINER | UNIT_NPC_FLAG_TRAINER_CLASS
WHERE `entry` IN (252359,252373,252377,252379,252382,254082,254084,254086,254087,254088);

DELETE FROM `creature_trainer` WHERE `CreatureID` IN (251389,252373,252377,252379,252382,254082,254086,254087,254088);
INSERT INTO `creature_trainer` (`CreatureID`,`TrainerID`,`MenuID`,`OptionID`) VALUES
(251389,25,0,0),  -- Akeri Duskblade, rogue (starting)
(252373,1,0,0),   -- Anathamaas Aetherwind, mage
(252377,23,0,0),  -- Seena Skybreaker, warrior
(252379,26,0,0),  -- Eltheen Nightbreeze, rogue
(252382,10,0,0),  -- Sessaria Skystride, shaman
(254082,10,0,0),  -- Aarnor Galestrike, shaman
(254086,1,0,0),   -- Shenaan Spellwind, mage
(254087,26,0,0),  -- Miriaan Mistblade, rogue
(254088,23,0,0);  -- Corsan Earthrazer, warrior

-- The Skybreaker Bulwark (94003, the Skyborne warrior class quest from Seena Skybreaker): its quest item came from Zaal Stormshield
-- at a guessed 40% (the drop was never seen in the sniffs). The other Forever server drops it every time, as a named mob's quest item does.
UPDATE `creature_loot_template` SET `Chance`=100 WHERE `Entry`=257196 AND `Item`=263993;
