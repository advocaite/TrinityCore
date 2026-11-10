-- Classic 1.60 (WoW Forever): Northshire report (Discord): "Mining trainer doesn't work" and "new quests not offered in right order".
--
-- 1. Junior profession trainers. The sniff import added the junior trainers of the starting areas with the trainer npcflag and a
--    "Train me." gossip option, but only the ones whose list was opened in a sniff got a creature_trainer row (Walter Mason 267325
--    and Vartha Rockmane 267331 -> trainer 1223, Junior Herbalism Trainer 267332 -> 1221). The others have no trainer at all:
--    Creature::GetTrainerId() returns 0 and "Train me." opens nothing. Kelsey Fargo is the trainer the Forever quest Mining
--    Consultant (91745) sends the player to at the Echo Ridge Mine. Each junior trainer teaches the apprentice rank of its
--    profession: 1221 Herbalism (2372), 1223 Mining (2581), 1224 Skinning (8615), all from the same sniff
--    (2026_10_01_04_world_forever_sniff_npc.sql). The gossip option is the one of each creature's own menu where it has one
--    (39900 Kelsey, 39905 skinning, 44733 Sally, as Walter Mason); Nancy Songflower and Zor'la have no gossip flag or menu, so the
--    client asks for the list directly and the row without a menu is used.
-- 2. A Scribbled Letter (92479) is the letter from Tordrin Sternblade, the new human hunter trainer (its texts: "our local hunter
--    trainer", "ye'd not catch a human dead wit' a bow"). It was sniffed without a class mask (QuestSortID -81, which is the warrior
--    sort), so Marshal McBride offers it to every human after Kobold Camp Cleanup next to that class's own letter (3100-3105, which
--    have their class mask, as in VMaNGOS). Restrict it to hunters, like the Zephras class quests in 2026_10_23_00.
--
-- The vanilla Northshire chain (783, 7, 15, 21, 54, 5261, 33, 18, 6, 3903-3905, 3100-3105) is identical to VMaNGOS (world_full
-- 14 June 2021: PrevQuestId, NextQuestId, ExclusiveGroup, RequiredClasses), so nothing changes there. Safe to run again.

DELETE FROM `creature_trainer` WHERE `CreatureID` IN (247223,247226,247227,267329,267330,267336,267337);
INSERT INTO `creature_trainer` (`CreatureID`,`TrainerID`,`MenuID`,`OptionID`) VALUES
(247226,1223,39900,0), -- Kelsey Fargo, Junior Mining Trainer (Northshire, Echo Ridge Mine)
(247223,1221,0,0),     -- Nancy Songflower, Junior Herbalism Trainer (Northshire)
(247227,1224,39905,0), -- Riley Peltskinner, Junior Skinning Trainer (Northshire)
(267337,1223,44733,0), -- Sally Swiftwrench, Junior Mining Trainer
(267329,1221,0,0),     -- Zor'la, Junior Herbalism Trainer
(267330,1224,39905,0), -- Nawka Wildsong, Junior Skinning Trainer
(267336,1224,39905,0); -- Brighid Stormflayer, Junior Skinning Trainer

INSERT INTO `quest_template_addon` (`ID`, `AllowableClasses`) VALUES
(92479, 4) -- A Scribbled Letter (hunter)
ON DUPLICATE KEY UPDATE `AllowableClasses` = VALUES(`AllowableClasses`);
