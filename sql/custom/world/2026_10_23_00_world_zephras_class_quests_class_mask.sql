-- Zephras Isle (Thendal Grove): the class quests were sniffed without a class mask, so Rorian the Dayseeker offers
-- The Way of the Hunter (92482) to every class and the druid quest (92485) too. Restrict each to its own class.
-- Safe to run again.
INSERT INTO `quest_template_addon` (`ID`, `AllowableClasses`) VALUES
(92482, 4),    -- The Way of the Hunter
(92483, 8),    -- rogue class quest (QuestSortID -162)
(92484, 64),   -- shaman class quest (QuestSortID -82)
(92485, 1024)  -- druid class quest (QuestSortID -263)
ON DUPLICATE KEY UPDATE `AllowableClasses` = VALUES(`AllowableClasses`);
