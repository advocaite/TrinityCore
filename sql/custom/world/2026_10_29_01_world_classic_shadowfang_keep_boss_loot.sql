-- Classic 1.60: Shadowfang Keep bosses were missing their Forever blues (Rethilgore 273457+273456, Silverlaine 273637,
-- Nandos 273647, Fenrus 273646, Springvale 273643, Odo 273645); the single-use references are inlined together with
-- the new items so each boss drops exactly one of its own set per kill.
-- rollback: re-insert the reference pointers (3887,1,30131),(3927,1,30132),(4274,1,30139),(4278,1,30141),(4279,1,30142)
--   each with Chance=100,QuestRequired=0,GroupId=0,MinCount=1,MaxCount=1,Comment='VMaNGOS 1.12', re-insert
--   (3914,0,5254,100,0,1,0,1,1,'VMaNGOS 1.12'), then DELETE the rows inserted below.
DELETE FROM `creature_loot_template` WHERE `Entry` IN (3887,3927,4274,4278,4279) AND `ItemType` = 1 AND `Item` IN (30131,30132,30139,30141,30142);
DELETE FROM `creature_loot_template` WHERE `Entry` = 3914 AND `ItemType` = 0 AND `Item` = 5254 AND `GroupId` = 0;
DELETE FROM `creature_loot_template` WHERE `Entry` IN (3887,3927,4274,4278,4279,3914) AND `ItemType` = 0 AND `GroupId` = 1
  AND `Item` IN (5254,273457,273456,6321,6323,273637,3748,6314,273647,3230,6340,273646,3191,6320,273643,6318,6319,273645);
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(3914,0,5254,0,0,1,1,1,1,'Rethilgore - Rugged Spaulders'),
(3914,0,273457,0,0,1,1,1,1,'Rethilgore - Sorcerer Collar'),
(3914,0,273456,0,0,1,1,1,1,'Rethilgore - Cell Keeper\'s Claws'),
(3887,0,6321,0,0,1,1,1,1,'Baron Silverlaine - Silverlaine\'s Family Seal'),
(3887,0,6323,0,0,1,1,1,1,'Baron Silverlaine - Baron\'s Scepter'),
(3887,0,273637,0,0,1,1,1,1,'Baron Silverlaine - Blade of Silverlaine'),
(3927,0,3748,0,0,1,1,1,1,'Wolf Master Nandos - Feline Mantle'),
(3927,0,6314,0,0,1,1,1,1,'Wolf Master Nandos - Wolfmaster Cape'),
(3927,0,273647,0,0,1,1,1,1,'Wolf Master Nandos - Worgpelt Leggings'),
(4274,0,6340,0,0,1,1,1,1,'Fenrus the Devourer - Fenrus\' Hide'),
(4274,0,3230,0,0,1,1,1,1,'Fenrus the Devourer - Black Wolf Bracers'),
(4274,0,273646,0,0,1,1,1,1,'Fenrus the Devourer - Half-Eaten Boots'),
(4278,0,3191,0,0,1,1,1,1,'Commander Springvale - Arced War Axe'),
(4278,0,6320,0,0,1,1,1,1,'Commander Springvale - Commander\'s Crest'),
(4278,0,273643,0,0,1,1,1,1,'Commander Springvale - Worgenbane Talisman'),
(4279,0,6319,0,0,1,1,1,1,'Odo the Blindwatcher - Girdle of the Blindwatcher'),
(4279,0,6318,0,0,1,1,1,1,'Odo the Blindwatcher - Odo\'s Ley Staff'),
(4279,0,273645,0,0,1,1,1,1,'Odo the Blindwatcher - Blindwatcher\'s Sight');