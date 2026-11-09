-- WoW Forever enchanting lessons: A Pain in the Neck (96873, Horde, Pa'zula in Sen'jin Village) and An Enchanting Lesson
-- (91753, Alliance, Kitta Firewind in the Tower of Azora). Neither quest was in our world or in the other Forever server (Discord: Pa'zula has no
-- quest for a new enchanter). Built with the Wowhead Forever page tool (2026_10_26_39) from the pages: texts, level, XP, money,
-- reputation, giver and turn-in. Each asks for 3 Luminous Residue (247884), which comes from disenchanting the lesson item:
-- Hexed Pendants (275724) from the Echo Isles trolls, Crude Wax Effigies (247883) from Kobold Geomancers (the pages' summaries).
-- The 70532 client flags both lesson items ITEM_FLAG2_DISENCHANT_TO_LOOT_TABLE; the core now disenchants such an item into its
-- own disenchant_loot_template entry, keyed by its item id (Item::HasOwnDisenchantLoot).
-- Skill: Enchanting 1 to take them, as the Camping 101: Enchanting quests (the report: "once you learn enchanting").
-- INFERRED (no source has them): drop chance 35% (the tool's convention), drops only while the quest is in the log,
-- 1 residue per item.

-- 96873 A Pain in the Neck (level 8, required 5) -- Wowhead Forever page
DELETE FROM `quest_template` WHERE `ID`=96873;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestPackageID`,`ContentTuningID`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`RewardBonusMoney`,`RewardSpell`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`RewardFactionID1`,`RewardFactionOverride1`) VALUES (96873,2,0,0,14,0,0,0,4,1,0,1,0,0,0,0,8589934770,'A Pain in the Neck','Pa''zula in Sen''jin Village wants you to collect 3 Luminous Residue. Luminous Residue can be obtained by disenchanting Hexed Pendants dropped by trolls in the Echo Isles.','The trolls east of here have lost their will to the witchdoctor, Zalazane. He uses many cruel magics to keep hold of his prisoners'' minds, but one of his favorite tricks is to make them wear pendants wrapped in evil binding spells.$B$BPractice your enchanting skills by stripping the magic from these pendants, and bring me the results. And don''t accidentally put one around your neck, if you value your freedom!','','',76,7500);
DELETE FROM `quest_objectives` WHERE `QuestID`=96873;
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (9687300,96873,1,0,0,247884,3,0,0,0,0,0,1,'Luminous Residue',0);
DELETE FROM `quest_template_addon` WHERE `ID`=96873;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (96873,0,0,0);
DELETE FROM `quest_request_items` WHERE `ID`=96873;
INSERT INTO `quest_request_items` (`ID`,`CompletionText`,`VerifiedBuild`) VALUES (96873,'What do you have for me, $N?',0);
DELETE FROM `quest_offer_reward` WHERE `ID`=96873;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (96873,'Fine work. A shame about the slain trolls, but with the binding spells removed, their spirits can rest.',0);
DELETE FROM `quest_classic_money` WHERE `ID`=96873;
INSERT INTO `quest_classic_money` (`ID`,`RewardMoney`,`RewardMoneyMaxLevel`) VALUES (96873,175,0);
DELETE FROM `quest_classic_level` WHERE `ID`=96873;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (96873,8,5,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=96873;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (96873,2,8);
DELETE FROM `creature_queststarter` WHERE `quest`=96873;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (266881,96873,0);
DELETE FROM `creature_questender` WHERE `quest`=96873;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (266881,96873,0);
DELETE FROM `gameobject_queststarter` WHERE `quest`=96873;
DELETE FROM `gameobject_questender` WHERE `quest`=96873;
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry`=266881;

-- 91753 An Enchanting Lesson (level 7, required 5) -- Wowhead Forever page
DELETE FROM `quest_template` WHERE `ID`=91753;
INSERT INTO `quest_template` (`ID`,`QuestType`,`QuestPackageID`,`ContentTuningID`,`QuestSortID`,`QuestInfoID`,`SuggestedGroupNum`,`RewardNextQuest`,`RewardXPDifficulty`,`RewardXPMultiplier`,`RewardMoneyDifficulty`,`RewardMoneyMultiplier`,`RewardBonusMoney`,`RewardSpell`,`StartItem`,`Flags`,`AllowableRaces`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`QuestCompletionLog`,`RewardFactionID1`,`RewardFactionOverride1`) VALUES (91753,2,0,0,12,0,0,0,4,1,0,1,0,0,0,8,4294967372,'An Enchanting Lesson','Kitta Firewind in the Tower of Azora wants you to collect 3 Luminous Residue.$B$BLuminous Residue can be obtained by disenchanting Crude Wax Effigies dropped by Kobold Geomancers.','Academic rigor is of course essential to the enchanter''s craft, but a practical foundation can pay dividends. Consider, as an example, the lowly Kobold. Lacking formal education, they nevertheless demonstrate a sort of innate aptitude for simple cantrips and enchantments.$B$BObtain some of their wax effigies and deconstruct the enchantments bound within them. You may find it elucidating.','','',72,7500);
DELETE FROM `quest_objectives` WHERE `QuestID`=91753;
INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES (9175300,91753,1,0,0,247884,3,0,0,0,0,0,1,'Luminous Residue',0);
DELETE FROM `quest_template_addon` WHERE `ID`=91753;
INSERT INTO `quest_template_addon` (`ID`,`MaxLevel`,`AllowableClasses`,`PrevQuestID`) VALUES (91753,0,0,0);
DELETE FROM `quest_request_items` WHERE `ID`=91753;
INSERT INTO `quest_request_items` (`ID`,`CompletionText`,`VerifiedBuild`) VALUES (91753,'What have your studies revealed?',0);
DELETE FROM `quest_offer_reward` WHERE `ID`=91753;
INSERT INTO `quest_offer_reward` (`ID`,`RewardText`,`VerifiedBuild`) VALUES (91753,'Theory and practice are two sides of a coin, $N. Remember this.',0);
DELETE FROM `quest_classic_money` WHERE `ID`=91753;
INSERT INTO `quest_classic_money` (`ID`,`RewardMoney`,`RewardMoneyMaxLevel`) VALUES (91753,85,0);
DELETE FROM `quest_classic_level` WHERE `ID`=91753;
INSERT INTO `quest_classic_level` (`ID`,`QuestLevel`,`MinLevel`,`MaxLevel`) VALUES (91753,7,5,0);
DELETE FROM `quest_template_classic_level` WHERE `ID`=91753;
INSERT INTO `quest_template_classic_level` (`ID`,`LevelType`,`QuestLevel`) VALUES (91753,2,7);
DELETE FROM `creature_queststarter` WHERE `quest`=91753;
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES (11072,91753,0);
DELETE FROM `creature_questender` WHERE `quest`=91753;
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES (11072,91753,0);
DELETE FROM `gameobject_queststarter` WHERE `quest`=91753;
DELETE FROM `gameobject_questender` WHERE `quest`=91753;
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry`=11072;

-- Enchanting 1 to take them
UPDATE `quest_template_addon` SET `RequiredSkillID`=333, `RequiredSkillPoints`=1 WHERE `ID` IN (96873,91753);

-- the lesson items: drops (while the quest is in the log) and their own disenchant loot
DELETE FROM `creature_loot_template` WHERE `Entry` IN (3205,3206,3207) AND `Item`=275724;
DELETE FROM `creature_loot_template` WHERE `Entry`=476 AND `Item`=247883;
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(3205,0,275724,35,0,1,0,1,1,'Zalazane - Hexed Pendant (A Pain in the Neck, chance INFERRED)'),
(3206,0,275724,35,0,1,0,1,1,'Voodoo Troll - Hexed Pendant (A Pain in the Neck, chance INFERRED)'),
(3207,0,275724,35,0,1,0,1,1,'Hexed Troll - Hexed Pendant (A Pain in the Neck, chance INFERRED)'),
(476,0,247883,35,0,1,0,1,1,'Kobold Geomancer - Crude Wax Effigy (An Enchanting Lesson, chance INFERRED)');
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=1 AND ((`SourceGroup` IN (3205,3206,3207) AND `SourceEntry`=275724) OR (`SourceGroup`=476 AND `SourceEntry`=247883));
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,
`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
(1,3205,275724,0,0,9,0,96873,0,0,0,0,0,'','Hexed Pendant - drops while A Pain in the Neck is taken'),
(1,3206,275724,0,0,9,0,96873,0,0,0,0,0,'','Hexed Pendant - drops while A Pain in the Neck is taken'),
(1,3207,275724,0,0,9,0,96873,0,0,0,0,0,'','Hexed Pendant - drops while A Pain in the Neck is taken'),
(1,476,247883,0,0,9,0,91753,0,0,0,0,0,'','Crude Wax Effigy - drops while An Enchanting Lesson is taken');
DELETE FROM `disenchant_loot_template` WHERE `Entry` IN (275724,247883);
INSERT INTO `disenchant_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(275724,0,247884,100,0,1,0,1,1,'Hexed Pendant - Luminous Residue'),
(247883,0,247884,100,0,1,0,1,1,'Crude Wax Effigy - Luminous Residue');
