-- Classic 1.60: Landslide's reference 30360 holds three items that do not exist in the Forever universe (17734/17736/17737,
-- Wowhead tooltip "Entity not found") plus Fist of Stone, whose local id is 17733 (Wowhead 17943); the dead pointer is
-- replaced by a guaranteed single-item group so the boss drops his real set item. All other Maraudon bosses' sets
-- (177xx range) are missing from this client's item data and stay untouched pending an item import.
-- rollback: DELETE FROM creature_loot_template WHERE Entry=12203 AND ItemType=0 AND Item=17733 AND GroupId=1;
--   re-insert (12203,1,30360,100,0,1,0,1,1,'VMaNGOS 1.12') and .reload creature_loot_template.
DELETE FROM `creature_loot_template` WHERE `Entry` = 12203 AND `ItemType` = 1 AND `Item` = 30360;
DELETE FROM `creature_loot_template` WHERE `Entry` = 12203 AND `ItemType` = 0 AND `Item` = 17733 AND `GroupId` = 1;
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(12203,0,17733,0,0,1,1,1,1,'Landslide - Fist of Stone');