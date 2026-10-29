-- Discord #issues "Level 10 Hunter Class Quest": Josephine Carson (251507, Goldshire hunter trainer) is the giver of the new
-- human hunter chain Taming the Beast 94792 -> 94863 -> 94864 -> Training the Beast 94793 (their texts name her), but had no
-- creature_queststarter / creature_questender rows. She ends the three taming quests; the ender of 94793 is not known yet.
DELETE FROM `creature_queststarter` WHERE `id` = 251507 AND `quest` IN (94792, 94863, 94864, 94793);
INSERT INTO `creature_queststarter` (`id`, `quest`, `VerifiedBuild`) VALUES
(251507, 94792, 0), (251507, 94863, 0), (251507, 94864, 0), (251507, 94793, 0);
DELETE FROM `creature_questender` WHERE `id` = 251507 AND `quest` IN (94792, 94863, 94864);
INSERT INTO `creature_questender` (`id`, `quest`, `VerifiedBuild`) VALUES
(251507, 94792, 0), (251507, 94863, 0), (251507, 94864, 0);
