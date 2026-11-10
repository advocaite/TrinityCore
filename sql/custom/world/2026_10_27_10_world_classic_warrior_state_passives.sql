-- Warrior: Victorious State (DND) 402974 and Offensive State (DND) 1282735 light up Victory Rush and Overpower. The 70532
-- client lists both as automatic level 1 Defense abilities (SkillLineAbility 48279 and 57190, warrior only), so a level 1
-- warrior saw both procs before training either spell (Victory Rush is trained at 20, Overpower at 12).
-- Each passive now comes with its own spell, and the automatic grant waits until that spell is known.
DELETE FROM `spell_learn_spell` WHERE (`entry` = 402927 AND `SpellID` = 402974) OR (`entry` = 7384 AND `SpellID` = 1282735);
INSERT INTO `spell_learn_spell` (`entry`, `SpellID`, `Active`) VALUES
(402927, 402974, 1),
(7384, 1282735, 1);

DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 35 AND `SourceEntry` IN (48279, 57190);
INSERT INTO `conditions` (`SourceTypeOrReferenceId`, `SourceGroup`, `SourceEntry`, `SourceId`, `ElseGroup`, `ConditionTypeOrReference`, `ConditionTarget`, `ConditionValue1`, `ConditionValue2`, `ConditionValue3`, `NegativeCondition`, `ErrorType`, `ErrorTextId`, `ScriptName`, `Comment`) VALUES
(35, 0, 48279, 0, 0, 25, 0, 402927, 0, 0, 0, 0, 0, '', 'Victorious State (DND) - only once Victory Rush is known'),
(35, 0, 57190, 0, 0, 25, 0, 7384, 0, 0, 0, 0, 0, '', 'Offensive State (DND) - only once Overpower is known');
