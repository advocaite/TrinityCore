-- Tome of Divinity: a paladin who has handed in the trainer's referral can still get the book and start the next quest
-- (Discord, nexum_ipv 2026-11-12: "after Brother Wilhelm's referral Duthorian Rall offers nothing"; fix found by SIN).
-- The book handouts 1641 (Duthorian Rall, Stormwind) and 1645 (Ironforge) each had conditions "the referral is not taken"
-- (type 14 QUEST_NONE) on the referrals 2998/3681 and 2997/2999/3000. Once a referral was handed in it was no longer NONE, so
-- the handout never showed. They now take "not started or rewarded" (type 47 QUEST_STATE, mask 1|64); a referral still in the
-- log must be handed in first.
-- The book's own quests 1642 and 1646 had PrevQuestID 1641 / 1645. Those handouts are repeatable (SpecialFlags 1) and a
-- repeatable quest is never stored as rewarded, so the PrevQuestID could never be met. The book stays the only way to start
-- them. Safe to run again.
UPDATE `conditions` SET `ConditionTypeOrReference` = 47, `ConditionValue2` = 65,
    `Comment` = CONCAT('Tome of Divinity ', `SourceEntry`, ': referral ', `ConditionValue1`, ' must be unstarted or rewarded')
WHERE `SourceTypeOrReferenceId` = 19 AND `SourceGroup` = 0 AND `ElseGroup` = 0 AND `ConditionTypeOrReference` = 14
  AND `NegativeCondition` = 0 AND (`SourceEntry`, `ConditionValue1`) IN ((1641,2998),(1641,3681),(1645,2997),(1645,2999),(1645,3000));

UPDATE `quest_template_addon` SET `PrevQuestID` = 0 WHERE (`ID`, `PrevQuestID`) IN ((1642,1641),(1646,1645));
