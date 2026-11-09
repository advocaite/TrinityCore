-- Trainers whose "train" gossip option opens nothing (Discord 2026-11-05: Zamja, the Orgrimmar cooking trainer). Our sniff import
-- linked their sniffed trainer list to option 0 of their gossip menu, but the train option of these menus is option 1 or 3.
-- The server skips a trainer link to an option that does not exist, so these four had no trainer at all. Point the link at
-- the menu's real train option (the trainer lists stay the sniffed ones). Safe to run again.
--   Zamja 3399 (cooking, Orgrimmar)    menu 4747 option 3
--   Kil'hala 3484                      menu 4270 option 1
--   Thrag Stonehoof 10278              menu 2748 option 1
--   Kray 11047                         menu 4119 option 3
DELETE FROM `creature_trainer` WHERE `CreatureID` IN (3399, 3484, 10278, 11047);
INSERT INTO `creature_trainer` (`CreatureID`,`TrainerID`,`MenuID`,`OptionID`) VALUES
(3399,36,4747,3),
(3484,369,4270,1),
(10278,293,2748,1),
(11047,353,4119,3);
