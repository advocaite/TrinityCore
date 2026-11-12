-- Junior mining and skinning trainers of Teldrassil and Durotar opened their shop at once and could not train (Discord 2026-11-12,
-- gsk857: Fanorran Stilloak in Aldrassil, "and the skinning one does too"). Kagil 267327, Norzsh 267328, Terunne Bearshaper 267333
-- and Fanorran Stilloak 267334 came with the 11-05 quest data restore: trainer and vendor flags, a trainer and goods, but no gossip
-- menu, so a click went straight to the goods. The junior trainers the sniffs saw (Walter Mason 267325, Margaret Weaver 267324)
-- use the menus "Train me." / "I would like to buy from you." 44733 (mining) and 39905 (skinning); these four get the same menus,
-- their trainer is opened by that menu's "Train me." option (OptionID 0). INFERRED: their own live menu ids are not in our sniffs;
-- the menus are the same kind of NPC's. Safe to run again.
DELETE FROM `creature_template_gossip` WHERE `CreatureID` IN (267327, 267328, 267333, 267334);
INSERT INTO `creature_template_gossip` (`CreatureID`, `MenuID`, `VerifiedBuild`) VALUES
(267327, 39905, 0),   -- Kagil, Junior Skinning Trainer (Durotar)
(267328, 44733, 0),   -- Norzsh, Junior Mining Trainer (Durotar)
(267333, 39905, 0),   -- Terunne Bearshaper, Junior Skinning Trainer (Teldrassil)
(267334, 44733, 0);   -- Fanorran Stilloak, Junior Mining Trainer (Teldrassil)
UPDATE `creature_trainer` SET `MenuID` = 39905, `OptionID` = 0 WHERE `CreatureID` IN (267327, 267333) AND `TrainerID` = 1224;
UPDATE `creature_trainer` SET `MenuID` = 44733, `OptionID` = 0 WHERE `CreatureID` IN (267328, 267334) AND `TrainerID` = 1223;
