-- Classic 1.60: Call of Water 2986 (Horde shaman breadcrumb from Narm Skychaser, Thunder Bluff) could be taken but not handed in:
-- no creature or object ends it, here or in VMaNGOS. The quest text sends the shaman to Islen Waterseer in the Barrens, her reward
-- text is the one the quest carries, and she already ends the Alliance twin 2985 and starts the target quest 1530. Safe to run again.

DELETE FROM `creature_questender` WHERE `id` = 5901 AND `quest` = 2986;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (5901,2986,0);
