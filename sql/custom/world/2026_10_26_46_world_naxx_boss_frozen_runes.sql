-- Loot: Frozen Runes from the Naxxramas bosses that drop them on Wowhead (classic era drop counts; the other Forever server drops them too).
-- Heigan the Unclean: 353 of 3154 kills
-- Grand Widow Faerlina: 101 of 4061 kills
-- Anub'Rekhan: 216 of 4519 kills
-- Gothik the Harvester: 866 of 2263 kills
DELETE FROM `creature_loot_template` WHERE `Entry` IN (15936,15953,15956,16060) AND `ItemType`=0 AND `Item`=22682;
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(15936,0,22682,11.2,0,1,0,1,1,'Frozen Rune'),
(15953,0,22682,2.5,0,1,0,1,1,'Frozen Rune'),
(15956,0,22682,4.8,0,1,0,1,1,'Frozen Rune'),
(16060,0,22682,38.3,0,1,0,1,1,'Frozen Rune')
;
