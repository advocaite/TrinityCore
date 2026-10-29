-- Collapse identical duplicate loot rows: Ukorz's Jang'thraze (doubled when 2026_10_29_08 re-ran before it was idempotent), the
-- 4012-4015 quest item 277917 (dropped twice per kill) and gameobject 16577's reference 12008 (rolled twice, doubled in the baseline).
-- rollback: none needed (the removed rows were exact copies).
DELETE FROM `creature_loot_template` WHERE `Entry` = 7267 AND `ItemType` = 0 AND `Item` = 9380 AND `GroupId` = 1;
DELETE FROM `creature_loot_template` WHERE `Entry` IN (4012,4013,4014,4015) AND `ItemType` = 0 AND `Item` = 277917 AND `GroupId` = 0;
DELETE FROM `gameobject_loot_template` WHERE `Entry` = 16577 AND `ItemType` = 1 AND `Item` = 12008 AND `GroupId` = 0;
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(7267,0,9380,0,0,1,1,1,1,'Chief Ukorz Sandscalp - Jang\'thraze the Protector'),
(4012,0,277917,100,1,1,0,1,1,'quest item of the creature query (drop not seen in the sniffs)'),
(4013,0,277917,100,1,1,0,1,1,'quest item of the creature query (drop not seen in the sniffs)'),
(4014,0,277917,100,1,1,0,1,1,'quest item of the creature query (drop not seen in the sniffs)'),
(4015,0,277917,100,1,1,0,1,1,'quest item of the creature query (drop not seen in the sniffs)');
INSERT INTO `gameobject_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(16577,1,12008,100,0,1,0,1,1,'VMaNGOS 1.12');
