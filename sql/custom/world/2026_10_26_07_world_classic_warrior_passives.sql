-- Warrior passives (see the hotfixes part): each is learned with the spell it belongs to.
-- 402927 Victory Rush -> 402974 Victorious State (DND); 7384 Overpower -> 1282735 Offensive State (DND), which Overpower needs to light up.
DELETE FROM `spell_learn_spell` WHERE (`entry` = 402927 AND `SpellID` = 402974) OR (`entry` = 7384 AND `SpellID` = 1282735);
INSERT INTO `spell_learn_spell` (`entry`, `SpellID`, `Active`) VALUES
(402927, 402974, 1),
(7384, 1282735, 1);
