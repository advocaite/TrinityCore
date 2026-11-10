-- Fixes from the Questie project's hand-written Forever corrections (Questie team, Questie/QuestieDB,
-- src/corrections/Forever, master aa1d7e72, 2026-11-08, GPLv3). Safe to run again.

-- 1) Silverpine Forest: the Forever Arugal's Folly (98298) and Stop the Spread (98299) follow the classic Arugal's Folly (99)
--    at Dalar Dawnweaver. Ours had 98298 after Prove Your Worth (421), taken from a Wowhead page, and 98299 open to anyone.
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES
(98298,99),
(98299,99)
ON DUPLICATE KEY UPDATE `PrevQuestID`=VALUES(`PrevQuestID`);

-- 2) Ruins of Lordaeron: Questie also lists Skeletal Soldier, Ghoul and Flesh Golem as droppers of the Bloodied Insignia
--    (268535, quest starter). INFERRED chance: 4%, the rate the other Ruins mobs already have here (Questie has no rates).
DELETE FROM `creature_loot_template` WHERE `Item`=268535 AND `Entry` IN (250626,250627,255109);
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(250626,0,268535,4,0,1,0,1,1,'Bloodied Insignia - Ruins of Lordaeron quest starter (Questie droppers, chance inferred)'),
(250627,0,268535,4,0,1,0,1,1,'Bloodied Insignia - Ruins of Lordaeron quest starter (Questie droppers, chance inferred)'),
(255109,0,268535,4,0,1,0,1,1,'Bloodied Insignia - Ruins of Lordaeron quest starter (Questie droppers, chance inferred)');
