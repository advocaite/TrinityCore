-- Ghost Wolf: live trainers teach 1238640 instead of 2645 since build 70888 (world 2026_11_13_82). Characters (players and bots)
-- who learned 2645 get 1238640 in its place, and their action buttons follow. Safe to run again.
INSERT IGNORE INTO `character_spell` (`guid`,`spell`,`active`,`disabled`)
SELECT `guid`,1238640,`active`,`disabled` FROM `character_spell` WHERE `spell`=2645;
DELETE FROM `character_spell` WHERE `spell`=2645;
UPDATE `character_action` SET `action`=1238640 WHERE `action`=2645 AND `type`=0;
