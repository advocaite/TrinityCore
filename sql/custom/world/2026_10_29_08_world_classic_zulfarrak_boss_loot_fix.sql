-- Classic 1.60: Wowhead's Zul'Farrak set items (9476/9477/11086/18082/9379/9639/9640) do not exist in this client's item
-- data (armory_item and the server item store both lack them), so the rows inserted by 2026_10_29_07 are removed; the only
-- locally existing set item, Jang'thraze the Protector (9380, this client's id for Wowhead's 11086), becomes Ukorz's
-- guaranteed drop. Gahz'rilla's rows (9467/9469) match Wowhead and stay, inert until those items are imported.
-- rollback: DELETE FROM creature_loot_template WHERE Entry=7267 AND ItemType=0 AND Item=9380 AND GroupId=1;
--   re-insert the rows deleted below, then .reload creature_loot_template.
DELETE FROM `creature_loot_template` WHERE `Entry` IN (7267,7271,8127) AND `ItemType` = 0 AND `GroupId` = 1
  AND `Item` IN (9476,9477,11086,18082,9379,9639,9640,9380);
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(7267,0,9380,0,0,1,1,1,1,'Chief Ukorz Sandscalp - Jang\'thraze the Protector');