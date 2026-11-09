-- Hunters: Revive Pet (982), Feed Pet (6991) and Dismiss Pet (2641) come with Tame Beast (1515) and Call Pet (883).
-- No hunter trainer teaches Revive Pet to players (dev DB read 5 Nov: only Call Pet 883 at level 10 on every hunter trainer;
-- Feed Pet and Dismiss Pet only on the unused trainer 40), so a player's pet that died could never be brought back
-- (player report, 5 Nov). Bots were given these spells in code. Like live, they are now learned together with the pet:
-- at once for a hunter who learns Tame Beast or Call Pet, and at the next login for a hunter who already knows them
-- (dependent spells are added while the spell book loads).
DELETE FROM `spell_learn_spell` WHERE `entry` IN (1515, 883) AND `SpellID` IN (982, 6991, 2641);
INSERT INTO `spell_learn_spell` (`entry`, `SpellID`, `Active`) VALUES
(1515, 982, 1),
(1515, 6991, 1),
(1515, 2641, 1),
(883, 982, 1),
(883, 6991, 1),
(883, 2641, 1);
