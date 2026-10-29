-- Trainers whose "Train me." opened nothing: they have the gossip menu but no creature_trainer row.
-- Junior gathering trainers (1.60 starting zones teach the apprentice rank only): mining 1223, skinning 1224, herbalism 1221,
-- the lists and menus the working juniors already use.
DELETE FROM `creature_trainer` WHERE `CreatureID` IN (247226, 267337, 247227, 267330, 267336, 247223, 267329, 252373, 254086, 252379, 254087, 252377, 254088);
INSERT INTO `creature_trainer` (`CreatureID`, `TrainerID`, `MenuID`, `OptionID`) VALUES
(247226, 1223, 39900, 0),   -- Kelsey Fargo, Junior Mining Trainer (Northshire, Echo Ridge Mine)
(267337, 1223, 44733, 0),   -- Sally Swiftwrench, Junior Mining Trainer
(247227, 1224, 39905, 0),   -- Riley Peltskinner, Junior Skinning Trainer (Northshire)
(267330, 1224, 39905, 0),   -- Nawka Wildsong, Junior Skinning Trainer
(267336, 1224, 39905, 0),   -- Brighid Stormflayer, Junior Skinning Trainer
(247223, 1221, 0, 0),       -- Nancy Songflower, Junior Herbalism Trainer (no gossip menu, like the working herbalism juniors)
(267329, 1221, 0, 0),       -- Zor'la, Junior Herbalism Trainer
-- Zephras Isle class trainers with their class menu but no list
(252373, 1, 41122, 0),      -- Anathamaas Aetherwind, mage
(254086, 1, 41122, 0),      -- Shenaan Spellwind, mage
(252379, 26, 41124, 0),     -- Eltheen Nightbreeze, rogue
(254087, 26, 41124, 0),     -- Miriaan Mistblade, rogue
(252377, 23, 41126, 0),     -- Seena Skybreaker, warrior
(254088, 23, 41126, 0);     -- Corsan Earthrazer, warrior
UPDATE `creature_template` SET `npcflag` = `npcflag` | 48 WHERE `entry` IN (252373, 254086, 252379, 254087, 252377, 254088);   -- trainer + class trainer
