-- Old Wowhead-page loot rows swapped for the Questie Forever traces where both cover the same quest item
-- (Questie team, Questie/QuestieDB, GPLv3, traces/foreverItemTraces.lua). Questie says which creature drops an item,
-- not how often, so chances below are our usual quest-drop values (INFERRED), not Wowhead numbers.
-- Rows Questie does not cover stay as they are; they are listed in realm-data-sources-1101/WOWHEAD-ROWS-NOT-COVERED.md.

-- Gleaning Our Future 98310: the Wowhead pages put the Heart of Neru on all three constructs and no other heart anywhere.
-- Questie: Ados 271338 -> Heart of Ados 280395, Modr 271460 -> 280396, Golm 271462 -> 280397, Neru 271458 -> 280398.
-- One heart per construct, 100%. (These constructs have no creature_template in our world yet, so the quest still
-- needs them built; the loot is right once they exist.)
DELETE FROM `creature_loot_template` WHERE `ItemType`=0 AND `Item` IN (280395,280396,280397,280398) AND `Entry` IN (271338,271458,271460,271462);
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(271338,0,280395,100,1,1,0,1,1,'Ados - Heart of Ados (Questie Forever traces)'),
(271460,0,280396,100,1,1,0,1,1,'Modr - Heart of Modr (Questie Forever traces)'),
(271462,0,280397,100,1,1,0,1,1,'Golm - Heart of Golm (Questie Forever traces)'),
(271458,0,280398,100,1,1,0,1,1,'Neru - Heart of Neru (Questie Forever traces)');

-- Slowing the Orc Advance 86757: Questie has the swords only from Horde Grunts 11682 and the daggers only from
-- Horde Scouts 11680; the Wowhead pages gave both items to both. Cross rows removed, kept rows re-credited.
DELETE FROM `creature_loot_template` WHERE `ItemType`=0 AND ((`Entry`=11680 AND `Item`=279505) OR (`Entry`=11682 AND `Item`=279507));
UPDATE `creature_loot_template` SET `Comment`='Horde Grunt - Jagged Orcish Sword (Questie Forever traces; chance INFERRED)' WHERE `Entry`=11682 AND `ItemType`=0 AND `Item`=279505;
UPDATE `creature_loot_template` SET `Comment`='Horde Scout - Curved Orcish Dagger (Questie Forever traces; chance INFERRED)' WHERE `Entry`=11680 AND `ItemType`=0 AND `Item`=279507;

-- Stolen Blasting Powder 95213: Empty Powder Keg 268548 on Rockjaw Ambusher 1116 / Backbreaker 1118 had a Wowhead-count
-- chance (48, all kills) and was not quest-only. Questie confirms them (plus 1115/1117, added in _13): quest-only, 40.
UPDATE `creature_loot_template` SET `Chance`=40, `QuestRequired`=1, `Comment`='Rockjaw - Empty Powder Keg (Questie Forever traces; chance INFERRED)' WHERE `Entry` IN (1116,1118) AND `ItemType`=0 AND `Item`=268548;

-- Rows whose creature Questie confirms and whose chance was already our own inferred value (not a Wowhead count):
-- only the source note changes.
UPDATE `creature_loot_template` SET `Comment`='Maraudine - quest item (Questie Forever traces; chance INFERRED)' WHERE `Entry`=4654 AND `ItemType`=0 AND `Item`=254432 AND `Comment` LIKE 'Wowhead Forever quest %';
UPDATE `creature_loot_template` SET `Comment`='Maraudine - quest item (Questie Forever traces; chance INFERRED)' WHERE `Entry`=4655 AND `ItemType`=0 AND `Item`=254432 AND `Comment` LIKE 'Wowhead Forever quest %';
UPDATE `creature_loot_template` SET `Comment`='Maraudine - quest item (Questie Forever traces; chance INFERRED)' WHERE `Entry`=4656 AND `ItemType`=0 AND `Item`=254432 AND `Comment` LIKE 'Wowhead Forever quest %';
UPDATE `creature_loot_template` SET `Comment`='Maraudine - quest item (Questie Forever traces; chance INFERRED)' WHERE `Entry`=4657 AND `ItemType`=0 AND `Item`=254432 AND `Comment` LIKE 'Wowhead Forever quest %';
UPDATE `creature_loot_template` SET `Comment`='Maraudine - quest item (Questie Forever traces; chance INFERRED)' WHERE `Entry`=4659 AND `ItemType`=0 AND `Item`=254432 AND `Comment` LIKE 'Wowhead Forever quest %';
UPDATE `creature_loot_template` SET `Comment`='Razormaw - quest item (Questie Forever traces; chance INFERRED)' WHERE `Entry`=1018 AND `ItemType`=0 AND `Item`=280103 AND `Comment` LIKE 'Wowhead Forever quest %';
UPDATE `creature_loot_template` SET `Comment`='Razormaw - quest item (Questie Forever traces; chance INFERRED)' WHERE `Entry`=1019 AND `ItemType`=0 AND `Item`=280103 AND `Comment` LIKE 'Wowhead Forever quest %';
UPDATE `creature_loot_template` SET `Comment`='Stormheart - quest item (Questie Forever traces; chance INFERRED)' WHERE `Entry`=270094 AND `ItemType`=0 AND `Item`=279186 AND `Comment` LIKE 'Wowhead Forever quest %';
