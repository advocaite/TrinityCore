-- Russian story, progress and reward text for 2 more Forever quests that a Russian player reported (2026-11-07) as
-- showing English text on a Russian client: 92751 Detonation at a Distance, 97237 Shelf Picked.
-- (97234 Reading Room from the same report is already filled by 2026_11_07_98.)
-- Their Russian title and objective line were already in the database; the quest window, progress and reward text
-- stayed English.
-- Text: our own translation of the English text, using the Russian names already in the database. No official
-- Russian text was available to us for these quests; replace it with the official text if it turns up.
-- Only fills a missing or empty Russian field; an existing one is kept. Safe to run again.
SET NAMES utf8mb4;

INSERT INTO `quest_template_locale` (`ID`,`locale`,`LogTitle`,`LogDescription`,`QuestDescription`,`VerifiedBuild`) VALUES
(92751,'ruRU','Удаленная детонация','Отнесите дистанционный детонатор Спрайт Прыгозубчик в Квартал дворфов в Штормграде.','Хотите что-то взорвать? Уж не связано ли это с вашими делами в Луноречье? Не делайте такое удивленное лицо. ШРУ:7 в курсе всего, что происходит на землях Штормграда.$BЕсли вы собираетесь устроить взрыв в пещерах под Западным краем, лучше заранее оказаться подальше оттуда. Возьмите этот дистанционный детонатор – это новый опытный образец от наших собственных инженеров.$BИ еще: лучше не упоминайте обо мне при вашем друге-эльфе.',0),
(97237,'ruRU','Прямо с полки','Найдите все нужные книги, находящиеся где-то в библиотеке, а затем вернитесь к Рою Льюэлсу в парк Штормграда.','Я проверил наши запасы: все книги из списка мистера Льюэлса сейчас на месте.$B$BОднако снимать их с полок вам придется самостоятельно. Бдительность – часть долга библиотекаря, так что я вернусь к надзору за учетом.$B$BЕсли вы отнесете книги Рою от его имени, вы не будете отвечать за их возврат: по его записке вы выступаете официальным курьером книжного клуба. Эта ответственность лежит на их заведении.$B$BВсего доброго, $n.',0)
ON DUPLICATE KEY UPDATE
  `QuestDescription`=IF(`QuestDescription` IS NULL OR `QuestDescription`='',VALUES(`QuestDescription`),`QuestDescription`);

INSERT INTO `quest_request_items_locale` (`ID`,`locale`,`CompletionText`,`VerifiedBuild`) VALUES
(97237,'ruRU','Забыли, зачем пришли, едва переступив порог? Со всеми бывает.',0)
ON DUPLICATE KEY UPDATE
  `CompletionText`=IF(`CompletionText` IS NULL OR `CompletionText`='',VALUES(`CompletionText`),`CompletionText`);

INSERT INTO `quest_offer_reward_locale` (`ID`,`locale`,`RewardText`,`VerifiedBuild`) VALUES
(92751,'ruRU','Отлично, это тот самый детонатор, который нам нужен!',0),
(97237,'ruRU','Те самые! Некоторые даже пахнут чем-то заморским! Замечательно, не терпится живо обсудить эти сочинения.$B$BВ подвале, после закрытия, разумеется.$B$BСпасибо, что так быстро управились, $n. Как и обещал, угощение за счет заведения. Мы держим слово.',0)
ON DUPLICATE KEY UPDATE
  `RewardText`=IF(`RewardText` IS NULL OR `RewardText`='',VALUES(`RewardText`),`RewardText`);
