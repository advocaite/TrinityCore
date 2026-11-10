-- Thendal (Zephras Isle) class trainers had the trainer flag but no trainer, so no Train button.
-- The other Forever server changelog: "Blademaster Ren, Dorii Brightwhisper and Windshaper Boro in Thendal have a Train button again."
-- That server's spell lists equal our starting-zone trainers: warrior 22, mage 5, shaman 11.
-- (the other Forever server's mage list also has 1326053-1326055, which are not in the 70532 client.)
DELETE FROM `creature_trainer` WHERE `CreatureID` IN (251964,251379,251374);
INSERT INTO `creature_trainer` (`CreatureID`,`TrainerID`,`MenuID`,`OptionID`) VALUES
(251964,22,0,0), -- Blademaster Ren, Warrior Trainer
(251379,5,0,0),  -- Dorii Brightwhisper, Mage Trainer
(251374,11,0,0); -- Windshaper Boro, Shaman Trainer
