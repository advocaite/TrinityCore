-- WoW Forever: a Russian player's game cache folder of client 1.60.1.70888 (Cache\WDB\ruRU, Blizzard's live realm),
-- written 2026-11-12 00:27, sent as "Cache 4.rar". Read with tools/forever_cache. Blizzard's values, unchanged.
-- Russian texts: only text with Russian letters, only missing rows and empty fields are filled, existing text is kept,
-- only for quests, NPCs and objects our world has. Safe to run more than once.

-- 1. Creature type live sends.
UPDATE `creature_template` SET `type` = 12 WHERE `entry` = 7382; -- Orange Tabby, ours 1
UPDATE `creature_template` SET `type` = 12 WHERE `entry` = 7384; -- Cornish Rex, ours 1
UPDATE `creature_template` SET `type` = 6 WHERE `entry` = 272273; -- Lost Watcher, ours 7
UPDATE `creature_template` SET `type` = 6 WHERE `entry` = 272291; -- Lost Knight, ours 7

-- 2. Rank (elite, rare, rare elite) live sends.
UPDATE `creature_template` SET `Classification` = 1 WHERE `entry` = 1748; -- Highlord Bolvar Fordragon, ours 3
UPDATE `creature_template` SET `Classification` = 1 WHERE `entry` = 1756; -- Stormwind Royal Guard, ours 0

-- 3. NPCs ours had as warriors (no mana) that live gives a mana class.
UPDATE `creature_template` SET `unit_class` = 2 WHERE `entry` = 248464; -- Nimsy
UPDATE `creature_template` SET `unit_class` = 2 WHERE `entry` = 258139; -- Frothrik Saegrund
UPDATE `creature_template` SET `unit_class` = 2 WHERE `entry` = 258203; -- Hervdana Saegrund
UPDATE `creature_template` SET `unit_class` = 8 WHERE `entry` = 258785; -- High Priestess Mims
UPDATE `creature_template` SET `unit_class` = 2 WHERE `entry` = 271338; -- Ados
UPDATE `creature_template` SET `unit_class` = 4 WHERE `entry` = 271346; -- Dragonmaw Infiltrator
UPDATE `creature_template` SET `unit_class` = 8 WHERE `entry` = 271348; -- Dragonmaw Soulbinder
UPDATE `creature_template` SET `unit_class` = 2 WHERE `entry` = 271460; -- Modr
UPDATE `creature_template` SET `unit_class` = 2 WHERE `entry` = 271462; -- Golm
UPDATE `creature_template` SET `unit_class` = 2 WHERE `entry` = 271486; -- Wendigo Shaman
UPDATE `creature_template` SET `unit_class` = 2 WHERE `entry` = 272291; -- Lost Knight

-- 4. Models for NPCs that have none in our data. Only when the NPC has no model row at all.

-- 5. Russian NPC names and titles for 6 NPCs.
DROP TEMPORARY TABLE IF EXISTS `tmp_ruru_creature_template_locale`;
CREATE TEMPORARY TABLE `tmp_ruru_creature_template_locale` (`entry` INT UNSIGNED NOT NULL, `Name` MEDIUMTEXT, `NameAlt` MEDIUMTEXT, `Title` MEDIUMTEXT, `TitleAlt` MEDIUMTEXT, PRIMARY KEY (`entry`)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
INSERT INTO `tmp_ruru_creature_template_locale` (`entry`,`Name`,`NameAlt`,`Title`,`TitleAlt`) VALUES
(256797,'Сумеречный осквернитель','Сумеречная осквернительница','',''),
(258785,'Верховная жрица Мимс','','Наставница жрецов',''),
(271336,'Резун','','',''),
(271338,'Адос','','',''),
(271368,'Теневейная белка','','',''),
(271636,'Трюкач-долгоносик','','','');
UPDATE `creature_template_locale` l JOIN `tmp_ruru_creature_template_locale` t ON t.`entry`=l.`entry` AND l.`locale`='ruRU' SET l.`Name`=IF(COALESCE(l.`Name`,'')='',t.`Name`,l.`Name`), l.`NameAlt`=IF(COALESCE(l.`NameAlt`,'')='',t.`NameAlt`,l.`NameAlt`), l.`Title`=IF(COALESCE(l.`Title`,'')='',t.`Title`,l.`Title`), l.`TitleAlt`=IF(COALESCE(l.`TitleAlt`,'')='',t.`TitleAlt`,l.`TitleAlt`);
INSERT INTO `creature_template_locale` (`entry`,`locale`,`Name`,`NameAlt`,`Title`,`TitleAlt`,`VerifiedBuild`) SELECT t.`entry`,'ruRU',t.`Name`,t.`NameAlt`,t.`Title`,t.`TitleAlt`,70888 FROM `tmp_ruru_creature_template_locale` t JOIN `creature_template` b ON b.`entry`=t.`entry` LEFT JOIN `creature_template_locale` l ON l.`entry`=t.`entry` AND l.`locale`='ruRU' WHERE l.`entry` IS NULL;
DROP TEMPORARY TABLE `tmp_ruru_creature_template_locale`;

-- 6. Russian object names for 45 objects.
DROP TEMPORARY TABLE IF EXISTS `tmp_ruru_gameobject_template_locale`;
CREATE TEMPORARY TABLE `tmp_ruru_gameobject_template_locale` (`entry` INT UNSIGNED NOT NULL, `name` MEDIUMTEXT, `castBarCaption` MEDIUMTEXT, `unk1` MEDIUMTEXT, PRIMARY KEY (`entry`)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
INSERT INTO `tmp_ruru_gameobject_template_locale` (`entry`,`name`,`castBarCaption`,`unk1`) VALUES
(405519,'Установленный факел','',''),
(421087,'Свитки Гоаз','',''),
(424023,'Узкий терракотовый кувшин','',''),
(424024,'Пузатый терракотовый кувшин','',''),
(424025,'Кусок сыра','',''),
(424044,'Шерстяной ковер','',''),
(424058,'Колотушка','',''),
(424061,'Металлическая миска','',''),
(572614,'Ящик','',''),
(620408,'Ребенок-дворф','',''),
(620410,'Выпеченный хлеб','',''),
(620411,'Стул','',''),
(629596,'Сугроб','',''),
(630686,'Большой костер','',''),
(630689,'Большой костер','',''),
(630691,'Большой костер','',''),
(630694,'Большой костер','',''),
(651952,'Зачарованная лютня','',''),
(671481,'План нападения на Гавань Менетилов','',''),
(671483,'План нападения на Южнобережье','',''),
(671486,'План нападения на Телсамар','',''),
(671487,'План нападения на Стальгорн','',''),
(671518,'Изодранная книга заклинаний','',''),
(672330,'Снаряжение Драконьей Пасти','',''),
(672334,'Снаряжение Драконьей Пасти','',''),
(672336,'Снаряжение Драконьей Пасти','',''),
(672337,'Снаряжение Драконьей Пасти','',''),
(696451,'Каменная скамья','',''),
(696458,'Деревянный стул','',''),
(696459,'Деревянный стул','',''),
(696460,'Деревянный стул','',''),
(696461,'Деревянный стул','',''),
(696462,'Деревянный стул','',''),
(696463,'Деревянный стул','',''),
(696464,'Деревянный стул','',''),
(696465,'Деревянный стул','',''),
(696466,'Деревянный стул','',''),
(696467,'Деревянный стул','',''),
(696468,'Деревянный стул','',''),
(696469,'Деревянный стул','',''),
(696470,'Деревянный стул','',''),
(696471,'Деревянный стул','',''),
(696480,'Каменная скамья','',''),
(696481,'Каменная скамья','',''),
(696483,'Каменная скамья','','');
UPDATE `gameobject_template_locale` l JOIN `tmp_ruru_gameobject_template_locale` t ON t.`entry`=l.`entry` AND l.`locale`='ruRU' SET l.`name`=IF(COALESCE(l.`name`,'')='',t.`name`,l.`name`), l.`castBarCaption`=IF(COALESCE(l.`castBarCaption`,'')='',t.`castBarCaption`,l.`castBarCaption`), l.`unk1`=IF(COALESCE(l.`unk1`,'')='',t.`unk1`,l.`unk1`);
INSERT INTO `gameobject_template_locale` (`entry`,`locale`,`name`,`castBarCaption`,`unk1`,`VerifiedBuild`) SELECT t.`entry`,'ruRU',t.`name`,t.`castBarCaption`,t.`unk1`,70888 FROM `tmp_ruru_gameobject_template_locale` t JOIN `gameobject_template` b ON b.`entry`=t.`entry` LEFT JOIN `gameobject_template_locale` l ON l.`entry`=t.`entry` AND l.`locale`='ruRU' WHERE l.`entry` IS NULL;
DROP TEMPORARY TABLE `tmp_ruru_gameobject_template_locale`;

-- 7. Russian quest texts for 35 quests (missing rows, empty fields).
UPDATE `quest_template_locale` SET `LogTitle`=IF(COALESCE(`LogTitle`,'')='','Одеяние волшебного плетения',`LogTitle`) WHERE `ID`=1942 AND `locale`='ruRU';
UPDATE `quest_template_locale` SET `LogTitle`=IF(COALESCE(`LogTitle`,'')='','Работа для Чистера',`LogTitle`) WHERE `ID`=2953 AND `locale`='ruRU';
UPDATE `quest_template_locale` SET `LogTitle`=IF(COALESCE(`LogTitle`,'')='','Работа для Чистера',`LogTitle`) WHERE `ID`=4603 AND `locale`='ruRU';
UPDATE `quest_template_locale` SET `LogTitle`=IF(COALESCE(`LogTitle`,'')='','Чистер 5200!',`LogTitle`) WHERE `ID`=4605 AND `locale`='ruRU';
UPDATE `quest_template_locale` SET `LogTitle`=IF(COALESCE(`LogTitle`,'')='','Пожертвование: шерсть',`LogTitle`) WHERE `ID`=7791 AND `locale`='ruRU';
UPDATE `quest_template_locale` SET `LogTitle`=IF(COALESCE(`LogTitle`,'')='','Пожертвование: шелк',`LogTitle`) WHERE `ID`=7793 AND `locale`='ruRU';
UPDATE `quest_template_locale` SET `LogTitle`=IF(COALESCE(`LogTitle`,'')='','Пожертвование: шерсть',`LogTitle`) WHERE `ID`=7802 AND `locale`='ruRU';
UPDATE `quest_template_locale` SET `LogTitle`=IF(COALESCE(`LogTitle`,'')='','Пожертвование: шелк',`LogTitle`) WHERE `ID`=7803 AND `locale`='ruRU';
UPDATE `quest_template_locale` SET `LogTitle`=IF(COALESCE(`LogTitle`,'')='','Пожертвование: шерсть',`LogTitle`) WHERE `ID`=7807 AND `locale`='ruRU';
UPDATE `quest_template_locale` SET `LogTitle`=IF(COALESCE(`LogTitle`,'')='','Пожертвование: шелк',`LogTitle`) WHERE `ID`=7808 AND `locale`='ruRU';
DELETE FROM `quest_template_locale` WHERE `ID`=78146 AND `locale`='ruRU';
INSERT INTO `quest_template_locale` (`ID`,`locale`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`PortraitGiverText`,`PortraitGiverName`,`PortraitTurnInText`,`PortraitTurnInName`,`QuestCompletionLog`,`VerifiedBuild`) VALUES (78146,'ruRU','Свитки Гоаз','','','','','','','','',70888);
DELETE FROM `quest_template_locale` WHERE `ID`=87760 AND `locale`='ruRU';
INSERT INTO `quest_template_locale` (`ID`,`locale`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`PortraitGiverText`,`PortraitGiverName`,`PortraitTurnInText`,`PortraitTurnInName`,`QuestCompletionLog`,`VerifiedBuild`) VALUES (87760,'ruRU','Святой ныряльщик','Найдите укротительницу из клана Гремучей Чешуи, убейте ее и добудьте жезл беспредельной власти.','Пришли вести из Дарнаса, $n!\n\nИзучив образцы животных, мы узнали, что наги подчинили этих морских существ своей воле и заставляют их перевозить всадников и тяжелые грузы. А самых непокорных, старых и тяжело раненых они просто изгнали. Это чудовищно!\n\nНаш священный долг – остановить это!\n\nСилы наг обосновались возле руин Матистры. Должно быть, они охотятся на крепкозубов с ближайшего побережья. Найди их укротительницу, убей ее и принеси мне ее жестокие инструменты!','','','','','','',70888);
UPDATE `quest_template_locale` SET `LogTitle`=IF(COALESCE(`LogTitle`,'')='','Запечатанный ящик',`LogTitle`) WHERE `ID`=91900 AND `locale`='ruRU';
UPDATE `quest_template_locale` SET `LogTitle`=IF(COALESCE(`LogTitle`,'')='','Запечатанный ящик',`LogTitle`) WHERE `ID`=91905 AND `locale`='ruRU';
DELETE FROM `quest_template_locale` WHERE `ID`=92432 AND `locale`='ruRU';
INSERT INTO `quest_template_locale` (`ID`,`locale`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`PortraitGiverText`,`PortraitGiverName`,`PortraitTurnInText`,`PortraitTurnInName`,`QuestCompletionLog`,`VerifiedBuild`) VALUES (92432,'ruRU','Настораживающая просьба','Поговорите с копией верховного мага Модеры близ Даларана.','У меня дурные вести из Даларана. Что-то нарушило течение энергии в силовых линиях, и механизмы в городе стали вести себя непредсказуемо и агрессивно.\n\nМы пока не знаем, в чем заключается первопричина случившегося. Верховный совет приказал нам держать все в тайне, чтобы не навлечь на орден позор. Но мы не сможем вечно обманывать народ. Город в опасности!\n\nВерховный маг Модера отправила меня за помощью. Отыщи ее чародейский образ на озере Лордамер у Даларана.','','','','','','Поговорите с копией верховного мага Модеры близ Даларана.',70888);
DELETE FROM `quest_template_locale` WHERE `ID`=92457 AND `locale`='ruRU';
INSERT INTO `quest_template_locale` (`ID`,`locale`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`PortraitGiverText`,`PortraitGiverName`,`PortraitTurnInText`,`PortraitTurnInName`,`QuestCompletionLog`,`VerifiedBuild`) VALUES (92457,'ruRU','Алчущие чары','Убейте $1oa |4ищущий осколок:ищущих осколка:ищущих осколков;, $2oa |4насыщенный осколок:насыщенных осколка:насыщенных осколков; и $3oa |4отголосок:отголоска:отголосков; маны в стенах Даларана по просьбе верховного мага Модеры, ждущей за пределами города.','Катастрофа отразилась на многих наших слугах-автоматах и чарах по всему городу – включая те, что отвечают за его оборону. Выброс фрагментов чародейской энергии привлек диких элементалей, которые жаждут поглотить необузданную магию.\n\nМы здесь заняты укреплением защитных чар и другими серьезными проблемами, так что ты окажешь Кирин-Тору огромную услугу, если проредишь эту толпу элементалей.','','','','','','',70888);
DELETE FROM `quest_template_locale` WHERE `ID`=92458 AND `locale`='ruRU';
INSERT INTO `quest_template_locale` (`ID`,`locale`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`PortraitGiverText`,`PortraitGiverName`,`PortraitTurnInText`,`PortraitTurnInName`,`QuestCompletionLog`,`VerifiedBuild`) VALUES (92458,'ruRU','Источник сбоя','Близ Даларана добудьте частицу чар проекции верховного мага Модеры.','Что бы ни послужило причиной сбоя в силовых линиях, началось это на самом верху Аметистовой цитадели. Оттуда донесся оглушительный грохот, а затем нас захлестнула волна необузданной чародейской энергии. Все наши силы уходят исключительно на поддержание защитных чар.\n\nМы больше не можем медлить. Нам нужна помощь.\n\nМаги Кирин-Тора занимаются наведением порядка в самой цитадели, но тебе придется найти менее заметный способ туда проникнуть. Возьми этот ключ и сразу же направляйся в цитадель, когда проберешься в город. Я буду ждать тебя там. Мы должны положить конец этому хаосу.','','','','','','',70888);
DELETE FROM `quest_template_locale` WHERE `ID`=93224 AND `locale`='ruRU';
INSERT INTO `quest_template_locale` (`ID`,`locale`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`PortraitGiverText`,`PortraitGiverName`,`PortraitTurnInText`,`PortraitTurnInName`,`QuestCompletionLog`,`VerifiedBuild`) VALUES (93224,'ruRU','Резные тотемы кентавров','Добудьте $1oa |4резной тотем:резных тотема:резных тотемов; кентавров, убивая кентавров из любого клана в Пустошах, а затем принесите добычу капитану Пентигасту на Высоту Найджела.','Наверняка для тебя не сюрприз, что оба клана кентавров не особенно рвутся просить помощи у чужаков. Особенно когда речь идет о не-кентаврах, как ты и я.\n\nНо у нас есть одна общая черта: все мы понимаем, что деньги – это власть. Нам удалось выяснить, что местные кентавры обмениваются резными идолами. Если принесешь мне несколько образцов, я смогу сделать срочный заказ на производство таких же работ. Возможно, нам удастся выторговать себе преимущество.','','','','','','',70888);
UPDATE `quest_template_locale` SET `LogTitle`=IF(COALESCE(`LogTitle`,'')='','Вкусы переменчивы',`LogTitle`), `LogDescription`=IF(COALESCE(`LogDescription`,'')='','Отправляйтесь на раскопки в Болотине и принесите мясо чащобного ящера.',`LogDescription`), `QuestDescription`=IF(COALESCE(`QuestDescription`,'')='','Замджа считает, нам нужно больше домашней стряпни.\nОна говорит, если мы начнем подавать мясо ящеров, это привлечет публику, которая обычно от "Жаровни Борстана" нос воротит.\n\nИдея, скажем прямо, неплохая. Но мне хочется готовить нечто поднимающееся выше ароматов наших родных деревень. Мы же в Оргриммаре! Борстан должен сделать из знакомых рецептов что-нибудь грандиозное!\n\nОдин охотник недавно вернулся из Болотины с огромным когтем чащобного ящера, которые водятся там на раскопках.\n\nТы в деле?',`QuestDescription`) WHERE `ID`=95697 AND `locale`='ruRU';
UPDATE `quest_template_locale` SET `LogTitle`=IF(COALESCE(`LogTitle`,'')='','Жертва болота',`LogTitle`), `LogDescription`=IF(COALESCE(`LogDescription`,'')='','Вернитесь к Дорину Поющему Клинку.',`LogDescription`), `QuestDescription`=IF(COALESCE(`QuestDescription`,'')='','<Вы накрываете тело дворфа его же плащом.>\n\nОтправляйтесь в Приозерье и лично сообщите Дорину Поющему Клинку о том, что случилось с его братом на раскопках.',`QuestDescription`) WHERE `ID`=95795 AND `locale`='ruRU';
UPDATE `quest_template_locale` SET `LogTitle`=IF(COALESCE(`LogTitle`,'')='','Плетеное сердце',`LogTitle`), `LogDescription`=IF(COALESCE(`LogDescription`,'')='','Вернитесь к Кейтлин Грассман в Гавань Менетилов.',`LogDescription`), `QuestDescription`=IF(COALESCE(`QuestDescription`,'')='','Я немного отдохну, а потом буду отсюда выбираться.\n\n<Ардин достает из сумки несколько тростинок и быстро сплетает из них сердечко.>\n\nВозьми это и отнеси Кейтлин – пусть знает, что скоро я буду дома! Благодаря тебе путь свободен, так что я теперь соберу свои снопы и отправлюсь обратно по твоему следу.\n\nСпасибо тебе.',`QuestDescription`) WHERE `ID`=95809 AND `locale`='ruRU';
UPDATE `quest_template_locale` SET `LogTitle`=IF(COALESCE(`LogTitle`,'')='','Основы походной жизни: рыбная ловля',`LogTitle`), `LogDescription`=IF(COALESCE(`LogDescription`,'')='','Поговорите с Пакстоном Гантером у Заледеневшего озера в Дун Мороге, чтобы показать свои навыки и научиться создавать предмет для лагеря, соответствующий вашей профессии.',`LogDescription`), `QuestDescription`=IF(COALESCE(`QuestDescription`,'')='','Ты, может, об этом не $gзадумывался:задумывалась;, но навыки рыбной ловли могут очень пригодиться в лагере.\n\nСдается мне, я знаю кое-кого, кто может научить тебя создавать аквариум для лагеря. Он порадует тебя и других искателей приключений, ведь даже простые радости могут сделать нас сильнее. Уверена, ты и $gсам:сама; в этом убедишься.',`QuestDescription`), `AreaDescription`=IF(COALESCE(`AreaDescription`,'')='','Отыщите лагерь охотников',`AreaDescription`) WHERE `ID`=96050 AND `locale`='ruRU';
DELETE FROM `quest_template_locale` WHERE `ID`=96264 AND `locale`='ruRU';
INSERT INTO `quest_template_locale` (`ID`,`locale`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`PortraitGiverText`,`PortraitGiverName`,`PortraitTurnInText`,`PortraitTurnInName`,`QuestCompletionLog`,`VerifiedBuild`) VALUES (96264,'ruRU','Стихийная помощь','Победите Чарглу Остробок в лабиринтах Иглошкурых и выясните, что замышляют свинобразы.','Ты, значит, могучий шаман, да? $gОбучился:Обучилась; обращению со стихиями и хочешь применить эти знания? Могу дать один совет.\n\nЕсли хочешь, чтобы стихии тебя уважали, всегда возвращай долги. Они помогают тебе – ты помогаешь им. И как раз сейчас им очень нужна помощь в борьбе против тех, кто хочет использовать свое могущество во зло.\n\nЗемля дрожит, $n. Она говорит, что свинобразы собираются для проведения какого-то ритуала невероятной силы. Надо исследовать их лабиринт и выяснить, что они задумали. Принеси мне все, что найдешь.','','','','','','Принесите порванную записку Вейрику Громоцепу на нагорье Арати.',70888);
DELETE FROM `quest_template_locale` WHERE `ID`=96984 AND `locale`='ruRU';
INSERT INTO `quest_template_locale` (`ID`,`locale`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`PortraitGiverText`,`PortraitGiverName`,`PortraitTurnInText`,`PortraitTurnInName`,`QuestCompletionLog`,`VerifiedBuild`) VALUES (96984,'ruRU','Источник сбоя','Добудьте частицу чар по просьбе волхва Вурдена Сиянье Бездны на Мельнице Таррен.','Может, тебе наши дела и не интересны, но от лишней пары рук я точно не откажусь. К тому же, если Даларан падет, то потянет за собой и все остальное, включая ваш Подгород.\n\nЗначит, твой покровитель хочет попасть в город? Ладно. Я зачарую ключ, чтобы он деактивировал защиту и ты $gмог:могла; попасть в Даларан. Я буду ждать тебя в Аметистовой цитадели – именно там все это и началось. В царящем хаосе Кирин-Тору будет не до того, чтобы тебя останавливать.\n\nЗабирай что хочешь, но что бы ни случилось, наша главная задача – предотвратить катастрофу.','','','','','','',70888);
UPDATE `quest_template_locale` SET `LogTitle`=IF(COALESCE(`LogTitle`,'')='','Могильный рыцарь',`LogTitle`), `LogDescription`=IF(COALESCE(`LogDescription`,'')='','Победите в Даларане Атрексия Могильного Рыцаря по просьбе Мелизары с Мельницы Таррены.',`LogDescription`), `QuestDescription`=IF(COALESCE(`QuestDescription`,'')='','В самых недрах города магов на севере скрывается предатель нашего народа, некромант незаурядного таланта. Он поднимает армию нежити прямо под носом у Кирин-Тора.\n\nМне нет дела до судьбы города магов, $n, но этот могильный рыцарь плевать хотел на все принципы Отрекшихся. Положи этому конец. Покажи ему, что никто не скроется от карающей длани Темной Госпожи.',`QuestDescription`) WHERE `ID`=96986 AND `locale`='ruRU';
DELETE FROM `quest_template_locale` WHERE `ID`=96987 AND `locale`='ruRU';
INSERT INTO `quest_template_locale` (`ID`,`locale`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`PortraitGiverText`,`PortraitGiverName`,`PortraitTurnInText`,`PortraitTurnInName`,`QuestCompletionLog`,`VerifiedBuild`) VALUES (96987,'ruRU','Обучение на работе','Найдите в Даларане "Основание Даларана" и принесите Рекси Меднохватьс к входу в канализацию.','Гляди-ка, да это ж родственная душа-делец. Пробираешься в Даларан в такие смутные времена? Ай-яй-яй!\n\nТы мне уже нравишься!\n\nПока наши магически одаренные товарищи заняты своими делами, предлагаю поискать пару "потерявшихся" сокровищ. У них тут целые библиотеки забиты навороченными книжками по волшебству. Принеси мне какой-нибудь тайный фолиантик, и я его толкну за хорошие деньги. Прибыль пополам.\n\nДа не смотри ты на меня так! Можно подумать, ты сюда на экскурсию $gпришел:пришла;. Ну что, по рукам?','','','','','','',70888);
UPDATE `quest_template_locale` SET `LogTitle`=IF(COALESCE(`LogTitle`,'')='','Источник силы',`LogTitle`), `LogDescription`=IF(COALESCE(`LogDescription`,'')='','Добудьте в Даларане $1oa |4треснувший сердечник:треснувших сердечника:треснувших сердечников; часового и принесите их доктору Мартину Фелбену в Подгород.',`LogDescription`), `QuestDescription`=IF(COALESCE(`QuestDescription`,'')='','Раз уж Даларан сейчас в осаде своих собственных механизмов, самое время, скажем так, одолжить у них парочку образцов.\n\nТолько представь себе армию движимых магией поганищ! У Темной Госпожи не останется иного выбора, кроме как признать мой алхимический гений. Может, она даже назначит меня главой Королевского фармацевтического общества!\n\nОтправляйся в Даларан и достань несколько сердечников часовых. Вряд ли они расстанутся с ними добровольно, так что действуй по обстоятельствам.',`QuestDescription`) WHERE `ID`=96988 AND `locale`='ruRU';
DELETE FROM `quest_template_locale` WHERE `ID`=98155 AND `locale`='ruRU';
INSERT INTO `quest_template_locale` (`ID`,`locale`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`PortraitGiverText`,`PortraitGiverName`,`PortraitTurnInText`,`PortraitTurnInName`,`QuestCompletionLog`,`VerifiedBuild`) VALUES (98155,'ruRU','Безупречные шкуры паршивых гиен','Принесите $1oa |4безупречную шкуру паршивой гиены:безупречные шкуры паршивых гиен:безупречных шкур паршивых гиен; Кристи Грант.','У паршивых гиен в Тысяче Игл просто великолепные шкуры. Ни разу за все время путешествий мне еще не попадался такой красивый и блестящий мех!\n\nЯ хотела бы отправить несколько шкур сестрам в Штормград – они наверняка смогли бы изготовить из них что-то невероятное.\n\nУ меня нет боевого опыта, поэтому я была бы очень благодарна, если бы ты $gдобыл:добыла; для меня шкуры паршивых гиен.','','','','','','',70888);
UPDATE `quest_template_locale` SET `LogTitle`=IF(COALESCE(`LogTitle`,'')='','Эта земля принадлежала им',`LogTitle`), `LogDescription`=IF(COALESCE(`LogDescription`,'')='','Идите по дороге и отыщите Ховина Мягкое Перо к востоку от Гавани Менетилов.',`LogDescription`), `QuestDescription`=IF(COALESCE(`QuestDescription`,'')='','После войны Трех Кланов, навсегда положившей конец привычной жизни дворфов, клан Громового Молота поселился здесь и построил горную крепость Грим Батол.\n\nЭто было чудесное место. Куда ни посмотри, повсюду парили грифоны, пикируя к водоемам, чтобы поймать рыбу.\n\nК сожалению, спокойствие было недолгим.\n\nХовин Мягкое Перо – один из немногих представителей клана Громового Молота, вернувшихся в эти земли спустя долгое время. Он держится особняком, города сторонится. Следуй по дороге на восток – рано или поздно наткнешься на него. Ему определенно нужна помощь.',`QuestDescription`) WHERE `ID`=98230 AND `locale`='ruRU';
DELETE FROM `quest_template_locale` WHERE `ID`=98300 AND `locale`='ruRU';
INSERT INTO `quest_template_locale` (`ID`,`locale`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`PortraitGiverText`,`PortraitGiverName`,`PortraitTurnInText`,`PortraitTurnInName`,`QuestCompletionLog`,`VerifiedBuild`) VALUES (98300,'ruRU','Послание в Стальгорн','Доставьте послание от Ховина королю Магни Бронзобороду.','У меня тут еще полным-полно работы, $n, и мне нужно передать весточку моей жене Клайре в Заоблачный пик. А еще сообщить Фалстаду новости.\n\nНо это мое дело. Дело Громового Молота.\n\nТем не менее тебе следует сообщить обо всем королю Магни Бронзобороду в Стальгорне. Болотина расположена почти вплотную к его землям, и события тут не предвещают ничего хорошего. Отнеси ему это письмо.\n\nЧтобы добраться до Стальгорна, ты можешь пройти через Дун Альгаз или отправиться по воздуху из Гавани Менетилов.','','','','','','',70888);
DELETE FROM `quest_template_locale` WHERE `ID`=98313 AND `locale`='ruRU';
INSERT INTO `quest_template_locale` (`ID`,`locale`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`PortraitGiverText`,`PortraitGiverName`,`PortraitTurnInText`,`PortraitTurnInName`,`QuestCompletionLog`,`VerifiedBuild`) VALUES (98313,'ruRU','Для дальнейшего изучения','Отнесите коллекцию реликвий Вельгара историку Карнику в Стальгорн.','С чем бы мы ни имели тут дело, $n, это действительно нечто невероятное.\n\nЯ все для тебя аккуратно упакую. Отнеси предметы историку Карнику в Зал Исследователей в Стальгорне. Обычно его можно найти в глубине библиотеки, уткнувшегося носом в какой-нибудь том.\n\nЧтобы докопаться до истины, потребуется целая армия, но, судя по тому, что нам уже известно, оно того стоит!','','','','','','',70888);
DELETE FROM `quest_template_locale` WHERE `ID`=98463 AND `locale`='ruRU';
INSERT INTO `quest_template_locale` (`ID`,`locale`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`PortraitGiverText`,`PortraitGiverName`,`PortraitTurnInText`,`PortraitTurnInName`,`QuestCompletionLog`,`VerifiedBuild`) VALUES (98463,'ruRU','Предмет сплетен','Поговорите с Никсразом Наливайкой в таверне "Старый моряк" в Пиратской Бухте, что в Тернистой долине.','Слушай, ты $gодин:одна; из моих любимых посетителей, поэтому я тебе кое-что расскажу.\n\nЗнаешь, где устраивают настоящие вечеринки? В таверне "Старый моряк" в Пиратской Бухте. Да, в той самой Пиратской Бухте, что в Тернистой долине.\n\nНе смотри на меня так, я знаю, что там всем заправляют гоблины. Бармены много болтают, а посетители – еще больше. Я просто хочу сказать, что если тебе нужно как следует повеселиться, отправляйся туда.\n\nУ Никсраза Наливайки всегда найдется выпить, в этом не сомневайся.','','','','','','',70888);
UPDATE `quest_template_locale` SET `LogTitle`=IF(COALESCE(`LogTitle`,'')='','Доисторическая призма',`LogTitle`), `LogDescription`=IF(COALESCE(`LogDescription`,'')='','Отнесите реликвию титанов старшему исследователю Магелласу в Зал Исследователей в Стальгорне.',`LogDescription`), `QuestDescription`=IF(COALESCE(`QuestDescription`,'')='','С такими похожими на письмена титанов надписями на гранях ему одна дорога – в Зал Исследователей Стальгорна.\n\nХочется думать, что за это тебя упомянут в каталоге: "Нашедший: $n! Подлинность подтверждена геологом Вельгаром..." Эх, что-то меня понесло.\n\nСтарший исследователь Магеллас тебя примет и, учитывая изысканность твоей находки, наверняка щедро отблагодарит, если ты пожертвуешь ее музею.\n\nОтличная добыча!',`QuestDescription`) WHERE `ID`=98824 AND `locale`='ruRU';
DELETE FROM `quest_template_locale` WHERE `ID`=98895 AND `locale`='ruRU';
INSERT INTO `quest_template_locale` (`ID`,`locale`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`PortraitGiverText`,`PortraitGiverName`,`PortraitTurnInText`,`PortraitTurnInName`,`QuestCompletionLog`,`VerifiedBuild`) VALUES (98895,'ruRU','Стычка на островах Черного Копья','Принесите 3 почетных знака островов Черного Копья бригадному генералу Альянса рядом со входом на поля боя.','Борьба с налетчиками Черного Копья накаляется. Если мы не получим подмогу, то скоро потеряем острова.\n\nКак гордый солдат Альянса, ты не $gдолжен:должна; этого допустить. Принеси мне доказательство успеха на передовой, и будешь $gдостоин:достойна; называться героем Альянса!','','','','','','',70888);
DELETE FROM `quest_template_locale` WHERE `ID`=98896 AND `locale`='ruRU';
INSERT INTO `quest_template_locale` (`ID`,`locale`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,`PortraitGiverText`,`PortraitGiverName`,`PortraitTurnInText`,`PortraitTurnInName`,`QuestCompletionLog`,`VerifiedBuild`) VALUES (98896,'ruRU','Стычка на островах Черного Копья','','','','','','','','',70888);
