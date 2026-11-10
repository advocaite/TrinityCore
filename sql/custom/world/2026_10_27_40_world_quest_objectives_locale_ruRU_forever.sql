-- ruRU text for the Forever quest objectives that have their own English text and no Russian anywhere in our data (Russian client
-- report, Tornn 2026-10-26). Written by us; names follow the ruRU quest texts of the same quests and the client's place names.
-- Only ruRU rows: the English quest_objectives text is not touched. Safe to run again.
SET NAMES utf8mb4;
DELETE FROM `quest_objectives_locale` WHERE `locale`='ruRU' AND `ID` IN (462329,462436,462437,462438,462439,464991,465190,465193,465282,465404,465686,465935,466031,466633,467563,467567,467938,468023,468024,468449,468541,468803,471712,471841,471923,471934,472272,472273,472284,473136,473689,473690,473693,473694,473697,473698,473701,473702,473705,473706,473779,473818,473843,475750,476525,476529,476533,476547,476559,476590,476596,476601,476603,476605,476611,476613,476620,476624,476626,476628,476630,476632,476634,476636,476639,476641,476752,477055,477056,477057,477058,477266,477368,477504,478910,479017,479018,479019,479020,479025,479026,9400220,9400257,9400280,9400282,9400374,9400380,9400381,9400382,9400383,9847033,91500049,91500053,91500101,91500102,91500103,91500104);
INSERT INTO `quest_objectives_locale` (`ID`,`locale`,`QuestId`,`StorageIndex`,`Description`,`VerifiedBuild`) VALUES
(462329,'ruRU',91209,0,'Явитесь к Шари Стилвелл в Брилле',0), -- Report to Shari Stilwell in Brill
(462436,'ruRU',91294,0,'Поговорите с Хильдой Крушительницей',0), -- Speak with Hilda the Breaker
(462437,'ruRU',91294,1,'Поговорите с Джорином Кроугом',0), -- Speak with Jorin Croge
(462438,'ruRU',91294,2,'Поговорите с Андером Соллиденом',0), -- Speak with Ander Solliden
(462439,'ruRU',91294,3,'Поговорите с Данитой Морр',0), -- Speak with Danitha Morr
(464991,'ruRU',92514,4,'Поговорите с Иллайей Янтарный Ветер',0), -- Speak with Illaya Amberwind
(465190,'ruRU',92597,0,'Используйте способность "Чтение силовых линий" у силовой линии в Тендалской роще',0), -- Use your Read Ley Line ability near the Thendal Grove Ley Line
(465193,'ruRU',92598,0,'Используйте "Небесный взор" у слияния стихий',0), -- Use Skysight near the Elemental Convergence
(465282,'ruRU',92643,1,'Найдите сектанта-перебежчика',0), -- Find the Al'Aketh Turncoat
(465404,'ruRU',92679,2,'Найдите Аамелию Поле Ветров',0), -- Find Aamelia Windfield
(465686,'ruRU',92849,1,'Найдите Филлиона Огненная Дымка',0), -- Find Fillion Flamebreeze
(465935,'ruRU',92947,3,'Явитесь к Хюсаа Быстрый Бриз',0), -- Report to Hyusaa Quickbreeze
(466031,'ruRU',93065,2,'Найдите Валеннию Штормовой Кулак на дороге',0), -- Find Valennia on the Road
(466633,'ruRU',93461,5,'Поговорите с Ратирилом Солнечное Копье',0), -- Speak with Rathiril Sunlance
(467563,'ruRU',93835,0,'Сразитесь с Лортуной',0), -- Confront Lorthuna
(467567,'ruRU',93836,3,'Поговорите с Талаанисом Песня Теней',0), -- Speak with Talaanis Shadowsong
(467938,'ruRU',93926,0,'Проверьте западную сторожевую башню на нагорье Шен''дар',0), -- Check in on the Western Watchtower in the Shen'dar Highlands
(468023,'ruRU',93461,0,'Поговорите с хозяином таверны',0), -- Speak with the Innkeeper
(468024,'ruRU',92514,0,'Поговорите с хозяином таверны',0), -- Speak with the Innkeeper
(468449,'ruRU',94369,3,'Поговорите с Талаанисом Песня Теней',0), -- Speak with Talaanis Shadowsong
(468541,'ruRU',94414,1,'Осмотрите опорный столп',0), -- View the Anchor Pylon
(468803,'ruRU',94489,0,'Найдите Йорела Песнь Ветра',0), -- Find Jorel Windsinger
(471712,'ruRU',95647,0,'Узнайте, что случилось с Ардином Грассманом',0), -- Learn what happened to Ardin Grassman
(471841,'ruRU',95998,0,'Сядьте у костра',0), -- Sit near the campfire
(471923,'ruRU',96057,0,'Повысьте навык "Портняжное дело" до 20',0), -- Raise your tailoring skill to 20
(471934,'ruRU',95998,1,'Получите усиление "Улучшенный отдых"',0), -- Gain the Boosted Rest buff
(472272,'ruRU',96101,0,'Сядьте у костра',0), -- Sit near the campfire
(472273,'ruRU',96101,1,'Получите усиление "Улучшенный отдых"',0), -- Gain the Boosted Rest buff
(472284,'ruRU',96102,0,'Повысьте навык "Портняжное дело" до 20',0), -- Raise your tailoring skill to 20
(473136,'ruRU',96392,0,'Воспользуйтесь дальним зрением Фарсена',0), -- Use Farsen's Far Sight
(473689,'ruRU',96604,2,'Сядьте у костра',0), -- Sit near the campfire
(473690,'ruRU',96604,3,'Получите усиление "Улучшенный отдых"',0), -- Gain the Boosted Rest buff
(473693,'ruRU',96605,2,'Сядьте у костра, набрав /сесть',0), -- Use the /sit emote near the campfire
(473694,'ruRU',96605,3,'Получите усиление "Улучшенный отдых"',0), -- Gain the Boosted Rest buff
(473697,'ruRU',96606,2,'Сядьте у костра',0), -- Sit near the campfire
(473698,'ruRU',96606,3,'Получите усиление "Улучшенный отдых"',0), -- Gain the Boosted Rest buff
(473701,'ruRU',96607,2,'Сядьте у костра',0), -- Sit near the campfire
(473702,'ruRU',96607,3,'Получите усиление "Улучшенный отдых"',0), -- Gain the Boosted Rest buff
(473705,'ruRU',96608,2,'Сядьте у костра',0), -- Sit near the campfire
(473706,'ruRU',96608,3,'Получите усиление "Улучшенный отдых"',0), -- Gain the Boosted Rest buff
(473779,'ruRU',96626,1,'Изучите кулинарию у Томаса в Златоземье',0), -- Learn cooking from Tomas in Goldshire
(473818,'ruRU',96646,0,'Изучите кулинарию у Зеррила Нежный Ветерок в деревне Шен''дар',0), -- Learn cooking from Zerril Softbreeze in Shen'dar Village
(473843,'ruRU',96658,1,'Изучите кулинарию у Вильяма Пикмана в Брилле',0), -- Learn cooking from William Pickman in Brill
(475750,'ruRU',97292,1,'Шипящая сыворотка введена',0), -- Hissing Serum administered
(476525,'ruRU',97921,0,'Повысьте навык "Травничество" до 20',0), -- Raise your herbalism skill to 20
(476529,'ruRU',97923,0,'Повысьте навык "Горное дело" до 20',0), -- Raise your mining skill to 20
(476533,'ruRU',97925,0,'Повысьте навык "Портняжное дело" до 20',0), -- Raise your tailoring skill to 20
(476547,'ruRU',97932,0,'Повысьте навык "Рыбная ловля" до 20',0), -- Raise your fishing skill to 20
(476559,'ruRU',97937,0,'Повысьте навык "Портняжное дело" до 20',0), -- Raise your tailoring skill to 20
(476590,'ruRU',97950,0,'Повысьте навык "Портняжное дело" до 20',0), -- Raise your tailoring skill to 20
(476596,'ruRU',97953,0,'Повысьте навык "Наложение чар" до 20',0), -- Raise your enchanting skill to 20
(476601,'ruRU',97955,0,'Повысьте навык "Первая помощь" до 20',0), -- Raise your first aid skill to 20
(476603,'ruRU',97956,0,'Повысьте навык "Рыбная ловля" до 20',0), -- Raise your fishing skill to 20
(476605,'ruRU',97957,0,'Повысьте навык "Травничество" до 20',0), -- Raise your herbalism skill to 20
(476611,'ruRU',97960,0,'Повысьте навык "Снятие шкур" до 20',0), -- Raise your skinning skill to 20
(476613,'ruRU',97961,0,'Повысьте навык "Портняжное дело" до 20',0), -- Raise your tailoring skill to 20
(476620,'ruRU',97963,1,'Повысьте навык "Алхимия" до 20',0), -- Raise your alchemy skill to 20
(476624,'ruRU',97964,0,'Повысьте навык "Кузнечное дело" до 20',0), -- Raise your blacksmithing skill to 20
(476626,'ruRU',97965,0,'Повысьте навык "Первая помощь" до 20',0), -- Raise your first aid skill to 20
(476628,'ruRU',97967,0,'Повысьте навык "Рыбная ловля" до 20',0), -- Raise your fishing skill to 20
(476630,'ruRU',97968,0,'Повысьте навык "Травничество" до 20',0), -- Raise your herbalism skill to 20
(476632,'ruRU',97969,0,'Повысьте навык "Кожевничество" до 20',0), -- Raise your leatherworking skill to 20
(476634,'ruRU',97970,0,'Повысьте навык "Горное дело" до 20',0), -- Raise your mining skill to 20
(476636,'ruRU',97971,0,'Повысьте навык "Снятие шкур" до 20',0), -- Raise your skinning skill to 20
(476639,'ruRU',97972,0,'Повысьте навык "Портняжное дело" до 20',0), -- Raise your tailoring skill to 20
(476641,'ruRU',97973,0,'Повысьте навык "Портняжное дело" до 20',0), -- Raise your tailoring skill to 20
(476752,'ruRU',94947,0,'Пройдите через портал небеснорожденных в Штормград',0), -- Take the Skyborne Portal to Stormwind
(477055,'ruRU',98221,0,'План нападения на Гавань Менетилов уничтожен',0), -- Attack Plan: Menethil Harbor destroyed
(477056,'ruRU',98221,1,'План нападения на Телсамар уничтожен',0), -- Attack Plan: Thelsamar destroyed
(477057,'ruRU',98221,2,'План нападения на Южнобережье уничтожен',0), -- Attack Plan: Southshore destroyed
(477058,'ruRU',98221,3,'План нападения на Стальгорн уничтожен',0), -- Attack Plan: Ironforge destroyed
(477266,'ruRU',98284,0,'Повысьте навык "Наложение чар" до 20',0), -- Raise your enchanting skill to 20
(477368,'ruRU',98319,0,'Горный стрелок Корнелиус найден',0), -- Mountaineer Cornelius found
(477504,'ruRU',98386,0,'Яйцо тарантула уничтожено',0), -- Tarantula Egg destroyed
(478910,'ruRU',98903,0,'Одержите победу на островах Черного Копья',0), -- Win in Darkspear Islands
(479017,'ruRU',93963,0,'Получите указания у Рэндала Эмерсона',0), -- Receive Instructions from Randal Emerson
(479018,'ruRU',93963,1,'Поговорите с главным механиком Меггакрутом',0), -- Speak with High Tinker Mekkatorque
(479019,'ruRU',93963,2,'Поговорите с королем Магни Бронзобородом',0), -- Speak with King Magni Bronzebeard
(479020,'ruRU',93963,3,'Поговорите с Тирандой Шелест Ветра',0), -- Speak with Tyrande Whisperwind
(479025,'ruRU',93739,1,'Поговорите с Вол''джином',0), -- Speak with Vol'jin
(479026,'ruRU',93739,2,'Поговорите с Кэрном Кровавое Копыто',0), -- Speak with Cairne Bloodhoof
(9400220,'ruRU',95034,0,'Поговорите с Люминой Певицей Ветра',0), -- Speak with Lumina Windsinger
(9400257,'ruRU',92753,0,'Взрывчатка заложена',0), -- Explosives placed
(9400280,'ruRU',95125,0,'Посмотрите, как Отт кует клинок',0), -- Watch Ott craft the blade
(9400282,'ruRU',95508,0,'Помогите Уолтону',0), -- Assist Walton
(9400374,'ruRU',97222,0,'Груз для привратницкой доставлен',0), -- Gatehouse Shipment delivered
(9400380,'ruRU',98067,0,'Вход в глубины Крепости Кенария',0), -- Cenarion Hold depths entrance
(9400381,'ruRU',98067,1,'Банк Дарнаса',0), -- Darnassus Bank
(9400382,'ruRU',98067,2,'Таверна на Террасе Ремесленников',0), -- Craftsman's Terrace Inn
(9400383,'ruRU',98067,3,'Городские ворота',0), -- City Gate
(9847033,'ruRU',99411,0,'Кайл накормлен',0), -- Kyle Fed
(91500049,'ruRU',93062,0,'Изможденные кости упокоены',0), -- Haggard Bones laid to rest
(91500053,'ruRU',98906,0,'Одержите победу на островах Черного Копья',0), -- Win in Darkspear Islands
(91500101,'ruRU',99144,0,'Выведите Барета Рассветная Скала с заставы Алого ордена',0), -- Escort Bareth Dawnstone out of Crusader's Outpost
(91500102,'ruRU',96204,0,'Проводите Люмину Певицу Ветра в безопасное место',0), -- Escort Lumina Windsinger to safety
(91500103,'ruRU',99123,0,'Выведите Пал''джу с Утеса Колкаров',0), -- Escort Pal'juh out of Kolkar Crag.
(91500104,'ruRU',98430,0,'Выведите Перита Грозовое Копыто из Скалы Бледногривых',0); -- Escort Perith Stormhoof out of Palemane Rock
