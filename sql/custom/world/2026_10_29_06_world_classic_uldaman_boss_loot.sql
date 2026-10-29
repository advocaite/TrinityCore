-- Classic 1.60: Archaedas's reference 30117 points at two items (9413, 9418) that do not exist in the Forever universe
-- (client dump and hotfixes lack them, Wowhead tooltip "Entity not found", zero observed drops in 6,520 kills); the dead
-- pointer is replaced by a guaranteed single-item group with his real set item, Archaedic Stone 11118. All other Uldaman
-- bosses already drop their full sets via references and need no change.
-- rollback: DELETE FROM creature_loot_template WHERE Entry=2748 AND ItemType=0 AND Item=11118 AND GroupId=1;
--   re-insert (2748,1,30117,100,0,1,0,1,1,'VMaNGOS 1.12') and .reload creature_loot_template.
DELETE FROM `creature_loot_template` WHERE `Entry` = 2748 AND `ItemType` = 1 AND `Item` = 30117;
DELETE FROM `creature_loot_template` WHERE `Entry` = 2748 AND `ItemType` = 0 AND `Item` = 11118 AND `GroupId` = 1;
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(2748,0,11118,0,0,1,1,1,1,'Archaedas - Archaedic Stone');