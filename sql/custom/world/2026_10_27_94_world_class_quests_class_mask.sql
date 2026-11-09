-- Class quests open to every class: a Windshaper Skyborne hunter took The Great Ursera Spirit (94006) and Strength and Mercy
-- (94638), whose reward spell teaches Bear Form. The quests sniffed from Forever came without a class mask, so AllowableClasses
-- stays 0 and Player::SatisfyQuestClass lets every class take them. The class sort in quest_template (QuestSortID -263 druid,
-- -261 hunter, ...) is not checked by the core.
-- In the other Forever server every one of its 631 quests with a class sort has exactly that class as AllowableClasses, so a class sort means
-- that class only. Fill the mask from the sort wherever it is still 0 (the 10-23 file did this for four Thendal Grove quests;
-- 65 more Forever quests were left, among them the Skyborne druid, hunter, shaman and warrior chains). A mask already set is kept.
-- Safe to run again.
INSERT INTO `quest_template_addon` (`ID`, `AllowableClasses`)
SELECT `ID`, CASE `QuestSortID`
        WHEN  -81 THEN    1 -- warrior
        WHEN -141 THEN    2 -- paladin
        WHEN -261 THEN    4 -- hunter
        WHEN -162 THEN    8 -- rogue
        WHEN -262 THEN   16 -- priest
        WHEN  -82 THEN   64 -- shaman
        WHEN -161 THEN  128 -- mage
        WHEN  -61 THEN  256 -- warlock
        WHEN -263 THEN 1024 -- druid
    END
FROM `quest_template`
WHERE `QuestSortID` IN (-81, -141, -261, -162, -262, -82, -161, -61, -263)
ON DUPLICATE KEY UPDATE `AllowableClasses` = IF(`AllowableClasses` = 0, VALUES(`AllowableClasses`), `AllowableClasses`);

-- Call of Earth (Zephras Isle, Windshaper Skyborne shaman) is sorted under the zone, not the class: its first step 92466 is
-- shaman-only since the 10-24 quest givers file, the next two are not. The other Forever server has all three at 64.
INSERT INTO `quest_template_addon` (`ID`, `AllowableClasses`) VALUES
(92467, 64),
(92468, 64)
ON DUPLICATE KEY UPDATE `AllowableClasses` = VALUES(`AllowableClasses`);
