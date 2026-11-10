-- Loot: the sniff import (2026_10_01_04) marked some trade goods as quest drops (QuestRequired 1), so they dropped only for a
-- player on a quest that needs them. Linen Cloth was one of them on 33 humanoids (27 on Zephras Isle: the Al'Aketh, bandits and
-- seers, plus Tarnished and Shadowvale mobs), so a Skyborne could kill 200 humanoids on Zephras Isle and see no linen.
-- Linen Cloth, Small Egg, Strider Meat and Flask of Oil are ordinary drops in the vanilla tables (VMaNGOS) and in the sniffed
-- drop counts, so they drop for everyone again. The quest items of the same import keep their flag.
UPDATE `creature_loot_template` SET `QuestRequired`=0
WHERE `ItemType`=0 AND `Item` IN (2589,6889,5469,814) AND `QuestRequired`=1 AND `Comment` LIKE 'sniff:%';
