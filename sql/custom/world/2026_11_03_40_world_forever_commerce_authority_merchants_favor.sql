-- Azeroth Commerce Authority vendors in Redridge: their recipes, certifications, tabard, pet and mount cost Merchant's Favor
-- (currency 3402), not silver. Follows Stondry Darkhammer (2026_11_03_01). Items and Favor prices from the Forever vendor
-- lists of Alynsia (40), Fritz Fizzle (29), Kalsey Sanden (6 Favor items of 19), Tamelyn Aldridge (3) and Huey Sunnydale (1).
-- Item ids are the client's own (70707 ItemSparse, matched by name). Every ExtendedCost row is the client's own Merchant's Favor
-- row with no money: 11869 = 30, 11839/11859 = 45, 11871 = 60, 11872 = 90, 11841 = 120, 11842/11861 = 180, 11041 = 200,
-- 11843 = 240, 11862 = 270, 11863 = 360, 11522 = 1000, 11517 = 1200, 11879 = 1500, 11042 = 6000.
-- With an ExtendedCost set the server charges only the Favor (ItemExtendedCost Money is 0), no silver.
-- Rows the vendor already has get the Favor cost; rows it is missing are added. Kalsey's 13 silver/gold items are not touched.
-- Daniel Stitchsong, Mivin Shadowweave, Nina Surefire and Dianne Softstep: no sell list was available, nothing here for them.
-- Safe to run again.
DROP TEMPORARY TABLE IF EXISTS `tmp_ca_favor`;
CREATE TEMPORARY TABLE `tmp_ca_favor` (`entry` INT UNSIGNED NOT NULL, `slot` SMALLINT NOT NULL, `item` INT NOT NULL, `ExtendedCost` INT UNSIGNED NOT NULL, PRIMARY KEY (`entry`, `item`));
INSERT INTO `tmp_ca_favor` (`entry`, `slot`, `item`, `ExtendedCost`) VALUES
-- Alynsia <Enchanter>
(256732, 1, 271624, 11522), -- Enchanting Certification: 1000
(256732, 2, 249537, 11842), -- Formula: Brilliant Wand: 180
(256732, 3, 249507, 11841), -- Formula: Dreamstaff: 120
(256732, 4, 249526, 11843), -- Formula: Enchant 2H Weapon - Mighty Healing Power: 240
(256732, 5, 249525, 11843), -- Formula: Enchant 2H Weapon - Mighty Spell Power: 240
(256732, 6, 274394, 11843), -- Formula: Enchant 2H Weapon - Spellblasting: 240
(256732, 7, 274401, 11843), -- Formula: Enchant Bracer - Greater Spellpower: 240
(256732, 8, 249539, 11843), -- Formula: Enchant Bracer - Superior Deflection: 240
(256732, 9, 249538, 11843), -- Formula: Enchant Bracer - Superior Intellect: 240
(256732, 10, 249510, 11841), -- Formula: Enchant Chest - Absorption: 120
(256732, 11, 274399, 11843), -- Formula: Enchant Cloak - Agility: 240
(256732, 12, 249534, 11843), -- Formula: Enchant Gloves - Arcane Power: 240
(256732, 13, 249536, 11843), -- Formula: Enchant Gloves - Holy Power: 240
(256732, 14, 249535, 11843), -- Formula: Enchant Gloves - Natural Power: 240
(256732, 15, 274398, 11843), -- Formula: Enchant Gloves - Superior Strength: 240
(256732, 16, 249504, 11841), -- Formula: Enchant Necklace - Agility: 120
(256732, 17, 249505, 11841), -- Formula: Enchant Necklace - Deflection: 120
(256732, 18, 249503, 11841), -- Formula: Enchant Necklace - Healing Power: 120
(256732, 19, 249502, 11841), -- Formula: Enchant Necklace - Spell Power: 120
(256732, 20, 249496, 11841), -- Formula: Enchant Necklace - Strength: 120
(256732, 21, 274393, 11843), -- Formula: Enchant Off Hand - Wisdom: 240
(256732, 22, 249479, 11839), -- Formula: Enchant Weapon - Insight: 45
(256732, 23, 249480, 11839), -- Formula: Enchant Weapon - Recovery: 45
(256732, 24, 249481, 11839), -- Formula: Enchant Weapon - Revelation: 45
(256732, 25, 249482, 11839), -- Formula: Glimmering Staff: 45
(256732, 26, 249529, 11843), -- Formula: Idol of the Dream: 240
(256732, 27, 249530, 11843), -- Formula: Libram of Holy Alacrity: 240
(256732, 28, 249494, 11841), -- Formula: Libram of Invocation: 120
(256732, 29, 249476, 11839), -- Formula: Mystic Mushroom: 45
(256732, 30, 249484, 11839), -- Formula: Orb of Mystic Insight: 45
(256732, 31, 249485, 11839), -- Formula: Orb of Souls: 45
(256732, 32, 249478, 11839), -- Formula: Polished Driftwood Icon: 45
(256732, 33, 249506, 11841), -- Formula: Radiant Staff: 120
(256732, 34, 249483, 11839), -- Formula: Soulstaff: 45
(256732, 35, 249493, 11841), -- Formula: Talons of Wrath: 120
(256732, 36, 249477, 11839), -- Formula: Tenets of the Silver Hand: 45
(256732, 37, 249495, 11841), -- Formula: Totem of Ancestral Protection: 120
(256732, 38, 249532, 11843), -- Formula: Totem of Thunder: 240
(256732, 39, 249508, 11841), -- Formula: Truesilver Conduit: 120
(256732, 40, 249509, 11841), -- Formula: Twisting Essence Jar: 120
-- Fritz Fizzle <Engineer>
(256733, 1, 271625, 11522), -- Engineering Certification: 1000
(256733, 2, 280326, 11861), -- Schematic: Bent Goggles: 180
(256733, 3, 280323, 11861), -- Schematic: Clanking Cord: 180
(256733, 4, 264217, 11862), -- Schematic: Compact Critter Carrier: 270
(256733, 5, 280328, 11861), -- Schematic: Dented Goggles: 180
(256733, 6, 264216, 11862), -- Schematic: Emergency Field Cloak: 270
(256733, 7, 264205, 11859), -- Schematic: EZ-Thro Copper Bomb XL: 45
(256733, 8, 264211, 11861), -- Schematic: EZ-Thro Fireproof Fuse: 180
(256733, 9, 264204, 11859), -- Schematic: EZ-Thro Wrap: 45
(256733, 10, 280325, 11861), -- Schematic: Floppy Goggles: 180
(256733, 11, 280322, 11861), -- Schematic: Gizmo Girdle: 180
(256733, 12, 264221, 11862), -- Schematic: Gnomish Army Knife: 270
(256733, 13, 264234, 11863), -- Schematic: Gnomish Poultryizer: 360
(256733, 14, 264222, 11862), -- Schematic: Loot-A-Rang: 270
(256733, 15, 264206, 11859), -- Schematic: No Slip SAF-T Padding: 45
(256733, 16, 264209, 11861), -- Schematic: SAF-T Bell: 180
(256733, 17, 264203, 11859), -- Schematic: SAF-T Copper Bomb: 45
(256733, 18, 264220, 11862), -- Schematic: SAF-T Disposable Parachute: 270
(256733, 19, 264202, 11859), -- Schematic: SAF-T Dynamite: 45
(256733, 20, 264201, 11859), -- Schematic: SAF-T Tabs: 45
(256733, 21, 285286, 11861), -- Schematic: Satchel of Bronze Bombs: 180
(256733, 22, 285285, 11859), -- Schematic: Satchel of Copper Bombs: 45
(256733, 23, 285288, 11862), -- Schematic: Satchel of Dark Iron Bombs: 270
(256733, 24, 285287, 11862), -- Schematic: Satchel of Iron Bombs: 270
(256733, 25, 264237, 11863), -- Schematic: Stealthman 52: 360
(256733, 26, 280327, 11861), -- Schematic: Stuckbutton Goggles: 180
(256733, 27, 264235, 11863), -- Schematic: Ultralight Goblin Glider: 360
(256733, 28, 264232, 11863), -- Schematic: Ultrasafe Rechargeable Battery: 360
(256733, 29, 280321, 11861), -- Schematic: Whimsical Waistwrap: 180
-- Kalsey Sanden <Chef>
(256731, 1, 249888, 11871), -- Recipe: Calcified Smoothie: 60
(256731, 2, 249887, 11869), -- Recipe: Mrrggl Smrrthle: 30
(256731, 3, 249886, 11869), -- Recipe: Slimy Smoothie: 30
(256731, 4, 249889, 11871), -- Recipe: Spicy Smoothie: 60
(256731, 5, 249885, 11869), -- Recipe: Venomous Smoothie: 30
(256731, 6, 249890, 11872), -- Recipe: Wicked Smoothie: 90
-- Tamelyn Aldridge <Supply Officer>
(256389, 1, 270888, 11517), -- Advertising License Application: 1200
(256389, 2, 262765, 11041), -- Azeroth Commerce Authority Tabard: 200
(256389, 3, 284664, 11879), -- Packmule Treat: 1500
-- Huey Sunnydale <Mule Handler>
(256736, 1, 262766, 11042); -- Reins of the Pack Kodo: 6000
UPDATE `npc_vendor` v JOIN `tmp_ca_favor` t ON v.`entry` = t.`entry` AND v.`item` = t.`item` AND v.`type` = 1 SET v.`ExtendedCost` = t.`ExtendedCost`;
INSERT INTO `npc_vendor` (`entry`, `slot`, `item`, `maxcount`, `ExtendedCost`, `type`, `PlayerConditionID`, `IgnoreFiltering`, `VerifiedBuild`)
SELECT t.`entry`, t.`slot`, t.`item`, 0, t.`ExtendedCost`, 1, 0, 0, 0 FROM `tmp_ca_favor` t
WHERE NOT EXISTS (SELECT 1 FROM `npc_vendor` v WHERE v.`entry` = t.`entry` AND v.`item` = t.`item` AND v.`type` = 1);
DROP TEMPORARY TABLE `tmp_ca_favor`;
