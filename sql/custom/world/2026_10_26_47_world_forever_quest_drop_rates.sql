-- Loot: quest objective drops that _42 gave Wowhead's all-kill rate. Wowhead counts every kill, also by players without the quest,
-- so a quest-only item looks far rarer than it is (Curved Orcish Dagger 0.067%, Surgical Spidersilk 0.198%, Jagged Orcish Sword 0.386%,
-- Shadetooth's Thicket Raptor Meat 9.1%). These rows go back to the quest rate: 35% as the Wowhead quest pages set it (_39),
-- 48% for Thicket Raptor Meat as on the other Thicket raptors, and quest-only.
UPDATE `creature_loot_template` SET `Chance`=35, `QuestRequired`=1 WHERE `ItemType`=0 AND ((`Entry`=11680 AND `Item`=279507) OR (`Entry`=11682 AND `Item`=279505) OR (`Entry`=2563 AND `Item`=278225));
UPDATE `creature_loot_template` SET `Chance`=48, `QuestRequired`=1 WHERE `ItemType`=0 AND `Entry`=260325 AND `Item`=271100;
