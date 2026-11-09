-- Classic 1.60: mage scroll deciphering. Decipher (1296017) starts the skill (classic_spell_mage_decipher), the skill teaches the
-- four Decipher spells, and each one salvages the untranslated scrolls of its ItemSalvage list (client ItemSalvageLoot 70685):
--   255 (Decipher 1309973): Scroll: KWYJIBO 211780, CWAL 211785, CHAP BALK WELLES 211786, LOWER PING WHOMEVER 211787 (item level 10)
--   261 (Decipher 1309965): WUBBA WUBBA 211784, VOCE WELL 211853, OMIT KESA 211854, STHENIC LUNATE 211855 (item level 20)
--   262 (Decipher 1310029): UPDOG 213543, TOPAZ YORAK 213544, PEATCHY ATTAX 213545, SHOOBEEDOOP 213546, THAW WORDS 213547 (30)
--   263 (Decipher 1310030): DOST OREM 281015, FORGOT HOOF THUD 281016, KEEP CLEF FOCI 281017, RARE SERVICHI RADAR 281018 (50)
-- The server never handled the salvage effect and only a few creatures dropped these scrolls (sniff and wowhead rows of Zephras
-- Isle and the Ruins of Lordaeron, about 1 to 2 % per kill), so mages elsewhere never saw one.
-- INFERRED, no official data: what each scroll deciphers into (the client's scroll items of the next item level, one per decipher)
-- and the world drop (1.5 % per kill of any creature with loot, by creature level, mages only). Safe to run again.

-- 1) Decipher starts the skill
DELETE FROM `spell_script_names` WHERE `spell_id` = 1296017 AND `ScriptName` = 'classic_spell_mage_decipher';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES (1296017, 'classic_spell_mage_decipher');

-- 2) what an untranslated scroll deciphers into: one scroll of its band (the two rare quality scrolls less often)
DELETE FROM `item_loot_template` WHERE `Entry` IN (211780,211785,211786,211787,211784,211853,211854,211855,213543,213544,213545,213546,213547,281015,281016,281017,281018);
DROP TEMPORARY TABLE IF EXISTS `tmp_decipher`;
CREATE TEMPORARY TABLE `tmp_decipher` (`Scroll` int unsigned NOT NULL, `Band` smallint unsigned NOT NULL);
INSERT INTO `tmp_decipher` VALUES
(211780,255),(211785,255),(211786,255),(211787,255),
(211784,261),(211853,261),(211854,261),(211855,261),
(213543,262),(213544,262),(213545,262),(213546,262),(213547,262),
(281015,263),(281016,263),(281017,263),(281018,263);
DROP TEMPORARY TABLE IF EXISTS `tmp_decipher_result`;
CREATE TEMPORARY TABLE `tmp_decipher_result` (`Band` smallint unsigned NOT NULL, `Item` int unsigned NOT NULL, `Chance` float NOT NULL, `Name` varchar(64) NOT NULL);
INSERT INTO `tmp_decipher_result` VALUES
(255,274947,0,'Scroll of Imbue Lesser Flame'),(255,275067,0,'Scroll of Imbue Chillknife'),(255,275069,0,'Scroll of Rat Familiar'),
(261,277483,0,'Scroll of Frog Familiar'),(261,277484,0,'Scroll of Confuse Beast'),(261,277485,0,'Scroll of Imbue Frost'),
(261,277486,0,'Scroll of Imbue Striking'),(261,277487,0,'Scroll of Imbue Baleflame'),(261,277488,0,'Scroll of Imbue Iceknife'),
(261,277489,0,'Scroll of Imbue Spark'),(261,277490,3,'Scroll of the Saber'),
(262,277491,0,'Scroll of Rebuke Elemental'),(262,277492,0,'Scroll of Khadgar''s Unlocking'),(262,277493,0,'Scroll of Cat Familiar'),
(262,277494,0,'Scroll of Imbue Accuracy'),(262,277495,0,'Scroll of Imbue Quickening'),(262,277496,0,'Scroll of Imbue Balefrost'),
(262,277497,0,'Scroll of Imbue Flame'),(262,277498,0,'Scroll of Imbue Manablade'),
(263,277499,0,'Scroll of Greater Cryoblast'),(263,277500,0,'Scroll of Imbue Greater Flame'),(263,277501,0,'Scroll of Imbue Greater Frost'),
(263,277502,0,'Scroll of Imbue Precision'),(263,277503,0,'Scroll of Imbue Spellbreak'),(263,277504,3,'Scroll of Wit');
INSERT INTO `item_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`)
SELECT d.`Scroll`, 0, r.`Item`, r.`Chance`, 0, 1, 1, 1, 1, CONCAT('Deciphered: ', r.`Name`, ' (INFERRED)')
FROM `tmp_decipher` d JOIN `tmp_decipher_result` r ON r.`Band` = d.`Band`;

-- 3) world drop: one untranslated scroll of the creature's level band, for mages
DELETE FROM `reference_loot_template` WHERE `Entry` IN (9302550,9302610,9302620,9302630);
INSERT INTO `reference_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`)
SELECT 9300000 + d.`Band` * 10, 0, d.`Scroll`, 0, 0, 1, 1, 1, 1, 'Untranslated mage scroll (INFERRED world drop)'
FROM `tmp_decipher` d;
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 10 AND `SourceGroup` IN (9302550,9302610,9302620,9302630);
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`)
SELECT 10, 9300000 + d.`Band` * 10, d.`Scroll`, 0, 0, 15, 0, 128, 0, 0, '', 0, 0, 0, '', 'Untranslated mage scroll - mages only'
FROM `tmp_decipher` d;

DELETE FROM `creature_loot_template` WHERE `ItemType` = 1 AND `Item` IN (9302550,9302610,9302620,9302630);
INSERT IGNORE INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`)
SELECT DISTINCT ctd.`LootID`, 1,
       CASE WHEN l.`level_min` <= 15 THEN 9302550 WHEN l.`level_min` <= 25 THEN 9302610 WHEN l.`level_min` <= 40 THEN 9302620 ELSE 9302630 END,
       1.5, 0, 1, 0, 1, 1, 'Untranslated mage scroll (INFERRED world drop)'
FROM `creature_template_difficulty` ctd
JOIN `creature_classic_level` l ON l.`entry` = ctd.`Entry`
WHERE ctd.`DifficultyID` = 0 AND ctd.`LootID` <> 0
  AND EXISTS (SELECT 1 FROM `creature_loot_template` c WHERE c.`Entry` = ctd.`LootID`)
  AND NOT EXISTS (SELECT 1 FROM `creature_loot_template` c WHERE c.`Entry` = ctd.`LootID` AND c.`ItemType` = 0
                  AND c.`Item` IN (211780,211785,211786,211787,211784,211853,211854,211855,213543,213544,213545,213546,213547,281015,281016,281017,281018));

DROP TEMPORARY TABLE IF EXISTS `tmp_decipher`;
DROP TEMPORARY TABLE IF EXISTS `tmp_decipher_result`;
