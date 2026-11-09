-- Loot: Forever items whose Wowhead Forever item page names a dropper we have, and which dropped nowhere here
-- (named rares, dungeon bosses, Den'dralass). Season of Discovery drops on those pages (season 2 data) are left out.
-- Chance = Wowhead's drops / kills. For items Forever added to old rares the kill count includes kills from before the item
-- existed, so these rates are low; they are what the data says. Each row is its own roll (GroupId 0).
DELETE FROM `creature_loot_template` WHERE `Entry`=250936 AND `ItemType`=0 AND `Item`=263935;
DELETE FROM `creature_loot_template` WHERE `Entry`=250936 AND `ItemType`=0 AND `Item`=263936;
DELETE FROM `creature_loot_template` WHERE `Entry`=643 AND `ItemType`=0 AND `Item`=273293;
DELETE FROM `creature_loot_template` WHERE `Entry`=7361 AND `ItemType`=0 AND `Item`=274042;
DELETE FROM `creature_loot_template` WHERE `Entry`=7361 AND `ItemType`=0 AND `Item`=274043;
DELETE FROM `creature_loot_template` WHERE `Entry`=6229 AND `ItemType`=0 AND `Item`=274049;
DELETE FROM `creature_loot_template` WHERE `Entry`=4422 AND `ItemType`=0 AND `Item`=274160;
DELETE FROM `creature_loot_template` WHERE `Entry`=4543 AND `ItemType`=0 AND `Item`=274291;
DELETE FROM `creature_loot_template` WHERE `Entry`=3974 AND `ItemType`=0 AND `Item`=274292;
DELETE FROM `creature_loot_template` WHERE `Entry`=6487 AND `ItemType`=0 AND `Item`=274293;
DELETE FROM `creature_loot_template` WHERE `Entry`=6487 AND `ItemType`=0 AND `Item`=274294;
DELETE FROM `creature_loot_template` WHERE `Entry`=14272 AND `ItemType`=0 AND `Item`=281925;
DELETE FROM `creature_loot_template` WHERE `Entry`=763 AND `ItemType`=0 AND `Item`=282003;
DELETE FROM `creature_loot_template` WHERE `Entry`=3581 AND `ItemType`=0 AND `Item`=282047;
DELETE FROM `creature_loot_template` WHERE `Entry`=14266 AND `ItemType`=0 AND `Item`=282636;
DELETE FROM `creature_loot_template` WHERE `Entry`=14268 AND `ItemType`=0 AND `Item`=282637;
DELETE FROM `creature_loot_template` WHERE `Entry`=14425 AND `ItemType`=0 AND `Item`=282654;
DELETE FROM `creature_loot_template` WHERE `Entry`=14275 AND `ItemType`=0 AND `Item`=282710;
DELETE FROM `creature_loot_template` WHERE `Entry`=14276 AND `ItemType`=0 AND `Item`=282716;
DELETE FROM `creature_loot_template` WHERE `Entry`=14278 AND `ItemType`=0 AND `Item`=282721;
DELETE FROM `creature_loot_template` WHERE `Entry`=14222 AND `ItemType`=0 AND `Item`=283253;
DELETE FROM `creature_loot_template` WHERE `Entry`=2191 AND `ItemType`=0 AND `Item`=284173;
DELETE FROM `creature_loot_template` WHERE `Entry`=2175 AND `ItemType`=0 AND `Item`=284174;
DELETE FROM `creature_loot_template` WHERE `Entry`=7015 AND `ItemType`=0 AND `Item`=284185;
DELETE FROM `creature_loot_template` WHERE `Entry`=2184 AND `ItemType`=0 AND `Item`=284193;
DELETE FROM `creature_loot_template` WHERE `Entry`=4030 AND `ItemType`=0 AND `Item`=284382;
DELETE FROM `creature_loot_template` WHERE `Entry`=4066 AND `ItemType`=0 AND `Item`=284383;
DELETE FROM `creature_loot_template` WHERE `Entry`=5932 AND `ItemType`=0 AND `Item`=284386;
DELETE FROM `creature_loot_template` WHERE `Entry`=5931 AND `ItemType`=0 AND `Item`=284400;
DELETE FROM `creature_loot_template` WHERE `Entry`=10644 AND `ItemType`=0 AND `Item`=284666;
DELETE FROM `creature_loot_template` WHERE `Entry`=5934 AND `ItemType`=0 AND `Item`=285190;
DELETE FROM `creature_loot_template` WHERE `Entry`=14426 AND `ItemType`=0 AND `Item`=285212;
DELETE FROM `creature_loot_template` WHERE `Entry`=5836 AND `ItemType`=0 AND `Item`=285282;
DELETE FROM `creature_loot_template` WHERE `Entry`=5837 AND `ItemType`=0 AND `Item`=285282;
DELETE FROM `creature_loot_template` WHERE `Entry`=5830 AND `ItemType`=0 AND `Item`=285282;
DELETE FROM `creature_loot_template` WHERE `Entry`=5841 AND `ItemType`=0 AND `Item`=285282;
DELETE FROM `creature_loot_template` WHERE `Entry`=5828 AND `ItemType`=0 AND `Item`=285331;
DELETE FROM `creature_loot_template` WHERE `Entry`=5864 AND `ItemType`=0 AND `Item`=285332;
DELETE FROM `creature_loot_template` WHERE `Entry`=7895 AND `ItemType`=0 AND `Item`=286639;
DELETE FROM `creature_loot_template` WHERE `Entry`=255072 AND `ItemType`=0 AND `Item`=286987;
DELETE FROM `creature_loot_template` WHERE `Entry`=269134 AND `ItemType`=0 AND `Item`=286987;
DELETE FROM `creature_loot_template` WHERE `Entry`=255066 AND `ItemType`=0 AND `Item`=286989;
DELETE FROM `creature_loot_template` WHERE `Entry`=269134 AND `ItemType`=0 AND `Item`=286989;
INSERT INTO `creature_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(250936,0,263935,7.14,0,1,0,1,1,'Den''dralass: Elunar Longstaff (1 of 14)'),
(250936,0,263936,28.57,0,1,0,1,1,'Den''dralass: Windforged Shortblade (4 of 14)'),
(643,0,273293,0.97,0,1,0,1,1,'Sneed: Bandsaw Wristbands (144 of 14846)'),
(7361,0,274042,2.04,0,1,0,1,1,'Grubbis: Skullduggery Belt (67 of 3279)'),
(7361,0,274043,2.04,0,1,0,1,1,'Grubbis: Irradiated Shield (67 of 3279)'),
(6229,0,274049,0.55,0,1,0,1,1,'Crowd Pummeler 9-60: Schematic: 9-60 Battery Pack (46 of 8328)'),
(4422,0,274160,2.96,0,1,0,1,1,'Agathelos the Raging: Quilrager Throwing Axe (164 of 5538)'),
(4543,0,274291,0.59,0,1,0,1,1,'Bloodmage Thalnos: Polished Skullcap (100 of 17059)'),
(3974,0,274292,0.12,0,1,0,1,1,'Houndmaster Loksey: Houndmaster Boomerang (22 of 18049)'),
(6487,0,274293,0.06,0,1,0,1,1,'Arcanist Doan: Spellsever Crossbow (11 of 19420)'),
(6487,0,274294,0.05,0,1,0,1,1,'Arcanist Doan: Library Walkers (9 of 19420)'),
(14272,0,281925,0.41,0,1,0,1,1,'Snarlflare: Whelpfire Wand (2 of 482)'),
(763,0,282003,0.59,0,1,0,1,1,'Lost One Chieftain: Lost Chieftain''s Greatsword (2 of 337)'),
(3581,0,282047,1.52,0,1,0,1,1,'Sewer Beast: Baby Crocolisk (2 of 132)'),
(14266,0,282636,0.31,0,1,0,1,1,'Shanda the Spinner: Silkspun Gloves (1 of 321)'),
(14268,0,282637,0.21,0,1,0,1,1,'Lord Condar: Shaking Egg (1 of 477)'),
(14425,0,282654,1.33,0,1,0,1,1,'Gnawbone: Gnawed Bone (1 of 75)'),
(14275,0,282710,0.54,0,1,0,1,1,'Tamra Stormpike: Dun Garok Rifle (2 of 371)'),
(14276,0,282716,0.60,0,1,0,1,1,'Scargil: Imbued Scaled Cuffs (2 of 336)'),
(14278,0,282721,0.28,0,1,0,1,1,'Ro''Bark: Gnoll Shaman''s Staff (1 of 356)'),
(14222,0,283253,0.81,0,1,0,1,1,'Araga: Denmother''s Hide (1 of 123)'),
(2191,0,284173,0.20,0,1,0,1,1,'Licillin: Impcaster''s Staff (2 of 976)'),
(2175,0,284174,0.28,0,1,0,1,1,'Shadowclaw: Serrated Shadowclaw (2 of 717)'),
(7015,0,284185,0.11,0,1,0,1,1,'Flagglemurk the Cruel: Greymist Gauntlets (1 of 948)'),
(2184,0,284193,0.19,0,1,0,1,1,'Lady Moongazer: Moongazer''s Wand (1 of 538)'),
(4030,0,284382,0.65,0,1,0,1,1,'Vengeful Ancient: Budding Leaf Belt (2 of 309)'),
(4066,0,284383,0.38,0,1,0,1,1,'Nal''taszar: Faerie Dragon''s Skin (1 of 263)'),
(5932,0,284386,0.17,0,1,0,1,1,'Taskmaster Whipfang: Whipfang''s Skinsearer (1 of 594)'),
(5931,0,284400,0.19,0,1,0,1,1,'Foreman Rigger: Foreman''s Helm (1 of 515)'),
(10644,0,284666,0.50,0,1,0,1,1,'Mist Howler: Wolf Whistle (1 of 200)'),
(5934,0,285190,0.63,0,1,0,1,1,'Heartrazor: Wyvern Heart Band (2 of 319)'),
(14426,0,285212,0.20,0,1,0,1,1,'Harb Foulmountain: Foulmountain Family Hammer (1 of 496)'),
(5836,0,285282,0.17,0,1,0,1,1,'Engineer Whirleygig: Jagged Knives (1 of 604)'),
(5837,0,285282,0.08,0,1,0,1,1,'Stonearm: Jagged Knives (1 of 1216)'),
(5830,0,285282,0.22,0,1,0,1,1,'Sister Rathtalon: Jagged Knives (2 of 905)'),
(5841,0,285282,0.14,0,1,0,1,1,'Rocklance: Jagged Knives (1 of 732)'),
(5828,0,285331,1.23,0,1,0,1,1,'Humar the Pridelord: Mark of the Pack Leader (1 of 81)'),
(5864,0,285332,0.88,0,1,0,1,1,'Swinegart Spearhide: Puncturing Spear (2 of 227)'),
(7895,0,286639,1.67,0,1,0,1,1,'Ambassador Bloodrage: Ambassador''s Bloodrobes (4 of 240)'),
(255072,0,286987,0.23,0,1,0,1,1,'Dark Caster: Violet Sorcerer''s Sandals (1 of 436)'),
(269134,0,286987,0.13,0,1,0,1,1,'Kirin Tor Necromancer: Violet Sorcerer''s Sandals (1 of 797)'),
(255066,0,286989,0.12,0,1,0,1,1,'Fallen Mage: Violet Sorcerer''s Wraps (1 of 836)'),
(269134,0,286989,0.13,0,1,0,1,1,'Kirin Tor Necromancer: Violet Sorcerer''s Wraps (1 of 797)');
