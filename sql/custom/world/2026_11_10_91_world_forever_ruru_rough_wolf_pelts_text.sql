-- Russian story, progress and reward text for Rough Wolf Pelts (91751, Helene Peltskinner), which a player reported
-- (Discord, 2026-11-10) as showing English text on a Russian client. The Russian game cache of the live realm also carries
-- this story text in English, so 2026_11_09_13 copied the English text into the Russian row.
-- Text: our own translation of the English text. Replace it with the official text if it turns up.
-- Fills the Russian field when it is missing, empty or still the English text; any other Russian text is kept. Safe to run again.
SET NAMES utf8mb4;

INSERT INTO `quest_template_locale` (`ID`,`locale`,`LogTitle`,`LogDescription`,`QuestDescription`,`VerifiedBuild`) VALUES
(91751,'ruRU','Грубые волчьи шкуры','Снимайте шкуры с волков в Элвиннском лесу, чтобы добыть 7 грубых волчьих шкур, а затем отнесите их Хелене Меховяз рядом со Златоземьем.','Ищете, где бы попрактиковаться в снятии шкур? Обычно с охотой на волков мне помогает муж, но сейчас он занят у аббатства. Мне бы не помешала помощь со шкурами, чтобы закончить заказы, над которыми я работаю.',0)
ON DUPLICATE KEY UPDATE
  `LogTitle`=IF(`LogTitle` IS NULL OR `LogTitle`='',VALUES(`LogTitle`),`LogTitle`),
  `LogDescription`=IF(`LogDescription` IS NULL OR `LogDescription`='',VALUES(`LogDescription`),`LogDescription`),
  `QuestDescription`=IF(`QuestDescription` IS NULL OR `QuestDescription`='' OR `QuestDescription` LIKE 'Are you looking for an opportunity%',VALUES(`QuestDescription`),`QuestDescription`);

INSERT INTO `quest_request_items_locale` (`ID`,`locale`,`CompletionText`,`VerifiedBuild`) VALUES
(91751,'ruRU','Как успехи с волчьими шкурами?',0)
ON DUPLICATE KEY UPDATE
  `CompletionText`=IF(`CompletionText` IS NULL OR `CompletionText`='' OR `CompletionText` LIKE 'Any luck with those wolf pelts%',VALUES(`CompletionText`),`CompletionText`);

INSERT INTO `quest_offer_reward_locale` (`ID`,`locale`,`RewardText`,`VerifiedBuild`) VALUES
(91751,'ruRU','Отличные шкуры, $C. У вас настоящий талант к охоте.',0)
ON DUPLICATE KEY UPDATE
  `RewardText`=IF(`RewardText` IS NULL OR `RewardText`='' OR `RewardText` LIKE 'Excellent work collecting these pelts%',VALUES(`RewardText`),`RewardText`);
