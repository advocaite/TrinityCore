-- Classic 1.60: Blackrock Spire bosses carried references whose items either do not exist or are not their drops on the
-- Forever server (Omokk, Wyrmthalak, Halycon, Crystal Fang, Jed, Ghok, Zigris, Drakkisath x3, Rend's buggy 10429/10430
-- rows that made him drop Beast loot and Valor spaulders, The Beast's doubled 30276 pointer, Goraluk's 30303); the dead
-- or contradicting pointers are removed and each boss's Wowhead-observed drops (plans, books, footlocker) are added at
-- their measured rates. Vosh'gajin, Voone and Emberseer keep their self-healed guaranteed tier-0 gloves.
-- rollback: re-insert every pointer row deleted below with Chance/GroupId as listed in the finding
--   (agents/LOOT-BOSS-AUDIT-FINDING.md, Blackrock Spire section), then DELETE the rows inserted here, and
--   .reload creature_loot_template.
DELETE FROM `creature_loot_template` WHERE `ItemType` = 1 AND (
  (`Entry` = 9196  AND `Item` = 30244) OR (`Entry` = 9568 AND `Item` = 30254) OR
  (`Entry` = 10220 AND `Item` = 30264) OR (`Entry` = 10376 AND `Item` = 30273) OR
  (`Entry` = 10509 AND `Item` = 30294) OR (`Entry` = 9718 AND `Item` = 30256) OR
  (`Entry` = 9736  AND `Item` = 30257) OR (`Entry` = 10363 AND `Item` IN (30270,30271,30272)) OR
  (`Entry` = 10429 AND `Item` IN (10429,10430)) OR (`Entry` = 10430 AND `Item` = 30276) OR
  (`Entry` = 10899 AND `Item` = 30303));
DELETE FROM `creature_loot_template` WHERE `ItemType` = 0 AND `Item` IN (23320,15730,24101,19227,13247,12835,12834,12837,228312)
  AND `Entry` IN (9816,10363,10430,9736,10899);
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(9816,0,23320,1.37,0,1,0,1,1,'Pyroguard Emberseer - Tablet of Flame Shock VI (wowhead: 633 of 46,476 kills)'),
(10363,0,15730,4.58,0,1,0,1,1,'General Drakkisath - Pattern: Red Dragonscale Breastplate (wowhead: 677 of 14,723 kills)'),
(10430,0,24101,13.14,0,1,0,1,1,'The Beast - Book of Ferocious Bite V (wowhead: 1,453 of 11,057 kills)'),
(10430,0,19227,6.49,0,1,0,1,1,'The Beast - Ace of Beasts (wowhead: 462 of 7,134 kills)'),
(9736,0,13247,55.39,0,1,0,1,1,'Quartermaster Zigris - Footlocker (wowhead: 1,348 of 2,430 kills)'),
(9736,0,12835,11.55,0,1,0,1,1,'Quartermaster Zigris - Plans: Annihilator (wowhead: 285 of 2,468 kills)'),
(10899,0,12834,7.96,0,1,0,1,1,'Goraluk Anvilcrack - Plans: Arcanite Champion (wowhead: 526 of 6,609 kills)'),
(10899,0,12837,7.94,0,1,0,1,1,'Goraluk Anvilcrack - Plans: Masterwork Stormhammer (wowhead: 527 of 6,641 kills)'),
(10899,0,228312,4.85,0,1,0,1,1,'Goraluk Anvilcrack - Plans: Invincible Mail (wowhead: 60 of 1,237 kills)');