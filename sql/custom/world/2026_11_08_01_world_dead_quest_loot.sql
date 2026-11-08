-- Backlog 0028: quest-only loot rows that no quest can unlock (QuestRequired = 1, no Type 1 objective, no ItemDrop), so they
-- never drop. Verified per item on Wowhead Classic and the cmangos 1.12 DB (2026-11-08):
-- - plain items (recipes, gems, a herb, a green) at 15-45% on the Defias of Westfall: VMaNGOS reference loot unrolled; the
--   items keep their real low-chance rows elsewhere, so the row goes, the flag is not cleared;
-- - 5505 Teronis' Journal: quest 1023 hands it out (StartItem), it is not looted;
-- - 1.12 items of quests that do not exist (895 Worgen Skull, 3744 'NYI', 1527 'Deprecated ...', the warlock 2018/2020 ...).
-- The 42 rows of 1.60 items (ids above 190000, sniff guesses of the fork) stay until their quests are known.

DELETE FROM `creature_loot_template` WHERE `Entry` = 2376 AND `ItemType` = 0 AND `Item` = 774 AND `QuestRequired` = 1; -- Малахит, plain
DELETE FROM `creature_loot_template` WHERE `Entry` = 6170 AND `ItemType` = 0 AND `Item` = 895 AND `QuestRequired` = 1; -- Череп воргена, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 205 AND `ItemType` = 0 AND `Item` = 895 AND `QuestRequired` = 1; -- Череп воргена, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 920 AND `ItemType` = 0 AND `Item` = 895 AND `QuestRequired` = 1; -- Череп воргена, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 205 AND `ItemType` = 0 AND `Item` = 896 AND `QuestRequired` = 1; -- Клык воргена, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 206 AND `ItemType` = 0 AND `Item` = 896 AND `QuestRequired` = 1; -- Клык воргена, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 6170 AND `ItemType` = 0 AND `Item` = 896 AND `QuestRequired` = 1; -- Клык воргена, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 533 AND `ItemType` = 0 AND `Item` = 896 AND `QuestRequired` = 1; -- Клык воргена, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 898 AND `ItemType` = 0 AND `Item` = 896 AND `QuestRequired` = 1; -- Клык воргена, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 920 AND `ItemType` = 0 AND `Item` = 896 AND `QuestRequired` = 1; -- Клык воргена, no quest in 1.12
DELETE FROM `gameobject_loot_template` WHERE `Entry` = 2216 AND `ItemType` = 0 AND `Item` = 1527 AND `QuestRequired` = 1; -- Испорченный клок сена, no quest in 1.12
DELETE FROM `gameobject_loot_template` WHERE `Entry` = 1697 AND `ItemType` = 0 AND `Item` = 1923 AND `QuestRequired` = 1; -- Сумка посланника, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 11561 AND `ItemType` = 0 AND `Item` = 2477 AND `QuestRequired` = 1; -- Череп опустошителя, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 1754 AND `ItemType` = 0 AND `Item` = 3516 AND `QuestRequired` = 1; -- Голова Лесковара, no quest in 1.12
DELETE FROM `gameobject_loot_template` WHERE `Entry` = 1711 AND `ItemType` = 0 AND `Item` = 3744 AND `QuestRequired` = 1; -- Подвеска Кровавого камня, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 2376 AND `ItemType` = 0 AND `Item` = 3818 AND `QuestRequired` = 1; -- Бледнолист, plain
DELETE FROM `creature_loot_template` WHERE `Entry` = 2376 AND `ItemType` = 0 AND `Item` = 3871 AND `QuestRequired` = 1; -- Чертеж: золотые чешуйчатые наплечники, plain
DELETE FROM `gameobject_loot_template` WHERE `Entry` = 2008 AND `ItemType` = 0 AND `Item` = 3929 AND `QuestRequired` = 1; -- Добыча Моури, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 3696 AND `ItemType` = 0 AND `Item` = 5389 AND `QuestRequired` = 1; -- Оскверненный тотем фурболгов, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 10642 AND `ItemType` = 0 AND `Item` = 5448 AND `QuestRequired` = 1; -- Треснувший браслет элементаля, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 3891 AND `ItemType` = 0 AND `Item` = 5505 AND `QuestRequired` = 1; -- Записи Терониса, start item
DELETE FROM `creature_loot_template` WHERE `Entry` = 4021 AND `ItemType` = 0 AND `Item` = 5681 AND `QuestRequired` = 1; -- Едкая слизь, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 2376 AND `ItemType` = 0 AND `Item` = 6390 AND `QuestRequired` = 1; -- Выкройка: стильная синяя рубашка, plain
DELETE FROM `creature_loot_template` WHERE `Entry` = 2375 AND `ItemType` = 0 AND `Item` = 6391 AND `QuestRequired` = 1; -- Выкройка: стильная зеленая рубашка, plain
DELETE FROM `creature_loot_template` WHERE `Entry` = 2377 AND `ItemType` = 0 AND `Item` = 6579 AND `QuestRequired` = 1; -- Наплеч поборника, plain
DELETE FROM `creature_loot_template` WHERE `Entry` = 6238 AND `ItemType` = 0 AND `Item` = 6927 AND `QuestRequired` = 1; -- Ухо Большого Билла, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 4702 AND `ItemType` = 0 AND `Item` = 7271 AND `QuestRequired` = 1; -- Безупречный бивень, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 4703 AND `ItemType` = 0 AND `Item` = 7271 AND `QuestRequired` = 1; -- Безупречный бивень, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 4700 AND `ItemType` = 0 AND `Item` = 7271 AND `QuestRequired` = 1; -- Безупречный бивень, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 14406 AND `ItemType` = 0 AND `Item` = 7271 AND `QuestRequired` = 1; -- Безупречный бивень, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 11597 AND `ItemType` = 0 AND `Item` = 7271 AND `QuestRequired` = 1; -- Безупречный бивень, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 4701 AND `ItemType` = 0 AND `Item` = 7271 AND `QuestRequired` = 1; -- Безупречный бивень, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 6606 AND `ItemType` = 0 AND `Item` = 7333 AND `QuestRequired` = 1; -- Свисток инспектора, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 2374 AND `ItemType` = 0 AND `Item` = 7364 AND `QuestRequired` = 1; -- Выкройка: тяжелые перчатки Земли, plain
DELETE FROM `creature_loot_template` WHERE `Entry` = 2375 AND `ItemType` = 0 AND `Item` = 7364 AND `QuestRequired` = 1; -- Выкройка: тяжелые перчатки Земли, plain
DELETE FROM `creature_loot_template` WHERE `Entry` = 2376 AND `ItemType` = 0 AND `Item` = 7364 AND `QuestRequired` = 1; -- Выкройка: тяжелые перчатки Земли, plain
DELETE FROM `creature_loot_template` WHERE `Entry` = 2377 AND `ItemType` = 0 AND `Item` = 7364 AND `QuestRequired` = 1; -- Выкройка: тяжелые перчатки Земли, plain
DELETE FROM `gameobject_loot_template` WHERE `Entry` = 4770 AND `ItemType` = 0 AND `Item` = 7389 AND `QuestRequired` = 1; -- Учетная книга Торговой Компании, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 4861 AND `ItemType` = 0 AND `Item` = 7679 AND `QuestRequired` = 1; -- Клык крылана-охотника, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 4862 AND `ItemType` = 0 AND `Item` = 7679 AND `QuestRequired` = 1; -- Клык крылана-охотника, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 4863 AND `ItemType` = 0 AND `Item` = 7680 AND `QuestRequired` = 1; -- Чешуя нефритового василиска, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 2376 AND `ItemType` = 0 AND `Item` = 7909 AND `QuestRequired` = 1; -- Аквамарин, plain
DELETE FROM `creature_loot_template` WHERE `Entry` = 2377 AND `ItemType` = 0 AND `Item` = 7909 AND `QuestRequired` = 1; -- Аквамарин, plain
DELETE FROM `creature_loot_template` WHERE `Entry` = 2376 AND `ItemType` = 0 AND `Item` = 10316 AND `QuestRequired` = 1; -- Выкройка: многоцветный килт, plain
DELETE FROM `creature_loot_template` WHERE `Entry` = 8581 AND `ItemType` = 0 AND `Item` = 10664 AND `QuestRequired` = 1; -- Записка волхву Римтори, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 8608 AND `ItemType` = 0 AND `Item` = 10664 AND `QuestRequired` = 1; -- Записка волхву Римтори, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 2375 AND `ItemType` = 0 AND `Item` = 11038 AND `QuestRequired` = 1; -- Формула чар для двуручного оружия: малый дух, plain
DELETE FROM `creature_loot_template` WHERE `Entry` = 2376 AND `ItemType` = 0 AND `Item` = 11038 AND `QuestRequired` = 1; -- Формула чар для двуручного оружия: малый дух, plain
DELETE FROM `creature_loot_template` WHERE `Entry` = 2659 AND `ItemType` = 0 AND `Item` = 11108 AND `QuestRequired` = 1; -- Выцветшая фотография, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 2726 AND `ItemType` = 0 AND `Item` = 11108 AND `QuestRequired` = 1; -- Выцветшая фотография, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 6505 AND `ItemType` = 0 AND `Item` = 11108 AND `QuestRequired` = 1; -- Выцветшая фотография, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 6506 AND `ItemType` = 0 AND `Item` = 11108 AND `QuestRequired` = 1; -- Выцветшая фотография, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 6507 AND `ItemType` = 0 AND `Item` = 11108 AND `QuestRequired` = 1; -- Выцветшая фотография, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 6517 AND `ItemType` = 0 AND `Item` = 11108 AND `QuestRequired` = 1; -- Выцветшая фотография, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 6520 AND `ItemType` = 0 AND `Item` = 11108 AND `QuestRequired` = 1; -- Выцветшая фотография, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 6521 AND `ItemType` = 0 AND `Item` = 11108 AND `QuestRequired` = 1; -- Выцветшая фотография, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 6556 AND `ItemType` = 0 AND `Item` = 11108 AND `QuestRequired` = 1; -- Выцветшая фотография, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 6557 AND `ItemType` = 0 AND `Item` = 11108 AND `QuestRequired` = 1; -- Выцветшая фотография, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 9118 AND `ItemType` = 0 AND `Item` = 11108 AND `QuestRequired` = 1; -- Выцветшая фотография, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 9163 AND `ItemType` = 0 AND `Item` = 11108 AND `QuestRequired` = 1; -- Выцветшая фотография, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 9164 AND `ItemType` = 0 AND `Item` = 11108 AND `QuestRequired` = 1; -- Выцветшая фотография, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 9167 AND `ItemType` = 0 AND `Item` = 11108 AND `QuestRequired` = 1; -- Выцветшая фотография, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 10660 AND `ItemType` = 0 AND `Item` = 11108 AND `QuestRequired` = 1; -- Выцветшая фотография, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 9456 AND `ItemType` = 0 AND `Item` = 11222 AND `QuestRequired` = 1; -- Голова Кром'зара, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 2377 AND `ItemType` = 0 AND `Item` = 12261 AND `QuestRequired` = 1; -- Чертеж: пылающий золотой клинок, plain
DELETE FROM `gameobject_loot_template` WHERE `Entry` = 14460 AND `ItemType` = 0 AND `Item` = 15586 AND `QuestRequired` = 1; -- Быстро растущий цветок, no quest in 1.12
DELETE FROM `gameobject_loot_template` WHERE `Entry` = 15920 AND `ItemType` = 0 AND `Item` = 17822 AND `QuestRequired` = 1; -- Карты клана Северного Волка, no quest in 1.12
DELETE FROM `gameobject_loot_template` WHERE `Entry` = 15921 AND `ItemType` = 0 AND `Item` = 17823 AND `QuestRequired` = 1; -- Грозовая Вершина: планы битвы, no quest in 1.12
DELETE FROM `gameobject_loot_template` WHERE `Entry` = 17299 AND `ItemType` = 0 AND `Item` = 19911 AND `QuestRequired` = 1; -- Сердце хлыстня, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 15043 AND `ItemType` = 0 AND `Item` = 19975 AND `QuestRequired` = 1; -- Зулианский грязнотинник, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 8957 AND `ItemType` = 0 AND `Item` = 20018 AND `QuestRequired` = 1; -- Шкура гризли злобных когтей, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 7857 AND `ItemType` = 0 AND `Item` = 20021 AND `QuestRequired` = 1; -- Золотая пиратская серьга, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 7855 AND `ItemType` = 0 AND `Item` = 20021 AND `QuestRequired` = 1; -- Золотая пиратская серьга, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 7896 AND `ItemType` = 0 AND `Item` = 20021 AND `QuestRequired` = 1; -- Золотая пиратская серьга, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 7856 AND `ItemType` = 0 AND `Item` = 20021 AND `QuestRequired` = 1; -- Золотая пиратская серьга, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 7858 AND `ItemType` = 0 AND `Item` = 20021 AND `QuestRequired` = 1; -- Золотая пиратская серьга, no quest in 1.12
DELETE FROM `creature_loot_template` WHERE `Entry` = 15685 AND `ItemType` = 0 AND `Item` = 20021 AND `QuestRequired` = 1; -- Золотая пиратская серьга, no quest in 1.12

-- 77 rows
