-- Classic 1.60: the Corrupted Whelps of Blackwing Lair (vanilla 1.12) are immune to the school of their colour. VMaNGOS gives them
-- that through a permanent aura (Immunity: Fire 7942, Nature 7941, Frost 7940 and a custom arcane one), which our script casts
-- too; this makes it creature data, like 2026_10_11_03 does for every other vanilla immunity, so the whelps do not depend on
-- those spells existing. Safe to run again.
DROP TEMPORARY TABLE IF EXISTS `tmp_classic_whelp_immunity`;
CREATE TEMPORARY TABLE `tmp_classic_whelp_immunity` (`entry` int unsigned NOT NULL PRIMARY KEY, `school` tinyint unsigned NOT NULL);
INSERT INTO `tmp_classic_whelp_immunity` (`entry`,`school`) VALUES
(14022,4),   -- Corrupted Red Whelp: fire
(14023,8),   -- Corrupted Green Whelp: nature
(14024,16),  -- Corrupted Blue Whelp: frost
(14025,64);  -- Corrupted Bronze Whelp: arcane
REPLACE INTO `creature_immunities` (`ID`,`SchoolMask`,`DispelTypeMask`,`MechanicsMask`,`Effects`,`Auras`,`ImmuneAoE`,`ImmuneChain`,`Comment`)
SELECT 5000000 + m.`entry`, m.`school`, COALESCE(i.`DispelTypeMask`, 0), COALESCE(i.`MechanicsMask`, 0), COALESCE(i.`Effects`, ''), COALESCE(i.`Auras`, ''), COALESCE(i.`ImmuneAoE`, 0), COALESCE(i.`ImmuneChain`, 0), CONCAT('vanilla ', t.`name`)
FROM `tmp_classic_whelp_immunity` m JOIN `creature_template` t ON t.`entry` = m.`entry`
LEFT JOIN `creature_immunities` i ON i.`ID` = IF(t.`CreatureImmunitiesId` BETWEEN 5000000 AND 5999999, 5000000 + t.`entry`, t.`CreatureImmunitiesId`);
UPDATE `creature_template` t JOIN `tmp_classic_whelp_immunity` m ON m.`entry` = t.`entry` SET t.`CreatureImmunitiesId` = 5000000 + t.`entry`;
DROP TEMPORARY TABLE `tmp_classic_whelp_immunity`;
