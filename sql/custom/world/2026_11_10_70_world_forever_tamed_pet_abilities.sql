-- Classic 1.60 (vanilla): a tamed beast keeps the abilities it had in the wild, and using them in combat can teach them to the
-- hunter (the Beast Training rank, 10% a use). Player report 10 Nov: a newly tamed pet had only auto attack, so beast mastery
-- hunters could neither learn nor teach the family abilities. Nothing gave a tamed beast any active ability: the retail code only
-- adds family passives and the creature's own summon spells, and every hunter pet ability rank is a "learned" SkillLineAbility.
-- Data: creature -> PetSpellDataId from VMaNGOS world (creature_template.pet_spell_list_id, newest patch row, world_full_14_june_2021),
-- PetSpellDataId -> pet spells from Blizzard's Classic Era client table CreatureSpellData (1.15.9.70576). 316 beasts, 57 pet spells,
-- every one of them in our Spell data and taught by exactly one Beast Training rank (skill line 261). Read by Pet::GetTameSpells.
-- Beasts that had no CreatureSpellData in vanilla (Mangy Wolf 525 and many low level ones) come with no ability, like vanilla.
-- New Forever beasts are not in this list. Safe to run again.
CREATE TABLE IF NOT EXISTS `classic_pet_tame_spell` (
  `CreatureEntry` int unsigned NOT NULL,
  `SpellId` int unsigned NOT NULL,
  PRIMARY KEY (`CreatureEntry`, `SpellId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Classic abilities a beast keeps when tamed';

DELETE FROM `classic_pet_tame_spell`;
INSERT INTO `classic_pet_tame_spell` (`CreatureEntry`, `SpellId`) VALUES
(30,17253), -- Forest Spider
(113,7371), -- Stonetusk Boar
(118,17255), -- Prowler
(119,7371), -- Longsnout
(154,24423), -- Greater Fleshripper
(157,26177), -- Goretusk
(330,7371), -- Princess
(345,26178), -- Bellygrub
(390,7371), -- Porcine Entourage
(442,17255), -- Tarantula
(454,26177), -- Young Goretusk
(471,17255), -- Mother Fang
(505,17256), -- Greater Tarantula
(521,17256), -- Lupos
(524,7371), -- Rockhide Boar
(547,26177), -- Great Goretusk
(569,17256), -- Green Recluse
(574,17257), -- Naraxis
(616,17256), -- Chatter
(628,17257), -- Black Ravager
(681,1754), -- Young Stranglethorn Tiger
(682,23099), -- Stranglethorn Tiger
(683,1754), -- Young Panther
(684,24450), -- Shadowmaw Panther
(708,7371), -- Small Crag Boar
(728,23109), -- Bhag'thera
(729,23099), -- Sin'Dall
(731,23109), -- King Bangalash
(731,16832), -- King Bangalash
(736,1754), -- Panther
(767,23099), -- Swamp Jaguar
(768,24450), -- Shadow Panther
(769,17259), -- Deathstrike Tarantula
(822,16828), -- Young Forest Bear
(833,17255), -- Coyote Packleader
(833,24604), -- Coyote Packleader
(834,17255), -- Coyote
(922,16832), -- Silt Crawler
(976,23099), -- Kurzen War Tiger
(1087,17259), -- Sawtooth Snapper
(1088,16832), -- Monstrous Crawler
(1108,26090), -- Mistvale Gorilla
(1112,17257), -- Leech Widow
(1114,26090), -- Jungle Thunderer
(1125,7371), -- Crag Boar
(1126,7371), -- Large Crag Boar
(1127,7371), -- Elder Crag Boar
(1129,16827), -- Black Bear
(1130,16828), -- Bjarn
(1131,17253), -- Winter Wolf
(1132,17255), -- Timber
(1133,17255), -- Starving Winter Wolf
(1138,17253), -- Snow Tracker Wolf
(1185,17256), -- Wood Lurker
(1189,16829), -- Black Bear Patriarch
(1190,7371), -- Mountain Boar
(1191,26177), -- Mangy Mountain Boar
(1192,26177), -- Elder Mountain Boar
(1195,17255), -- Forest Lurker
(1196,16827), -- Ice Claw Bear
(1199,1742), -- Juvenile Snow Leopard
(1216,16829), -- Shore Crawler
(1225,16829), -- Ol' Sooty
(1258,17257), -- Black Ravager Mastiff
(1258,24605), -- Black Ravager Mastiff
(1505,17253), -- Night Web Spider
(1509,17253), -- Ragged Scavenger
(1553,1742), -- Greater Duskbat
(1555,17255), -- Vicious Night Web Spider
(1557,26187), -- Elder Mistvale Gorilla
(1688,17253), -- Night Web Matriarch
(1689,7371), -- Scarred Crag Boar
(1693,17255), -- Loch Crocolisk
(1713,23109), -- Elder Shadowmaw Panther
(1713,24452), -- Elder Shadowmaw Panther
(1765,17255), -- Worg
(1765,24604), -- Worg
(1778,16828), -- Ferocious Grizzled Bear
(1809,23148), -- Carrion Vulture
(1809,24578), -- Carrion Vulture
(1816,3010), -- Diseased Grizzly
(1817,17260), -- Diseased Wolf
(1824,17260), -- Plague Lurker
(1922,17253), -- Gray Forest Wolf
(1923,17256), -- Bloodsnout Worg
(1961,16828), -- Mangeclaw
(1984,7371), -- Young Thistle Boar
(1985,7371), -- Thistle Boar
(1994,17253), -- Githyiss the Vile
(1995,16827), -- Strigid Owl
(1997,16828), -- Strigid Hunter
(1999,17253), -- Webwood Venomfang
(2000,17255), -- Webwood Silkspinner
(2001,17255), -- Giant Webwood Spider
(2042,1742), -- Nightsaber
(2070,1742), -- Moonstalker Runt
(2089,17257), -- Giant Wetlands Crocolisk
(2163,16828), -- Thistle Bear
(2232,16828), -- Tide Crawler
(2237,1753), -- Moonstalker Sire
(2321,1742), -- Foreststrider Fledgling
(2323,1753), -- Giant Foreststrider
(2348,17257), -- Elder Moss Creeper
(2349,17257), -- Giant Moss Creeper
(2350,17256), -- Forest Moss Creeper
(2351,16829), -- Gray Bear
(2384,1753), -- Starving Mountain Lion
(2385,1754), -- Feral Mountain Lion
(2406,24450), -- Mountain Lion
(2408,17257), -- Snapjaw
(2408,26064), -- Snapjaw
(2476,17256), -- Large Loch Crocolisk
(2505,17260), -- Saltwater Snapjaw
(2522,1756), -- Jaguero Stalker
(2522,24453), -- Jaguero Stalker
(2563,17258), -- Plains Creeper
(2565,17258), -- Giant Plains Creeper
(2578,23145), -- Young Mesa Buzzard
(2579,23145), -- Mesa Buzzard
(2681,17260), -- Vilebranch Raiding Wolf
(2681,23110), -- Vilebranch Raiding Wolf
(2686,17259), -- Witherbark Broodguard
(2727,17258), -- Crag Coyote
(2727,23099), -- Crag Coyote
(2728,23099), -- Feral Crag Coyote
(2729,23099), -- Elder Crag Coyote
(2729,24605), -- Elder Crag Coyote
(2730,23109), -- Rabid Crag Coyote
(2731,1755), -- Ridge Stalker
(2731,24450), -- Ridge Stalker
(2732,1755), -- Ridge Huntress
(2734,23109), -- Ridge Stalker Patriarch
(2734,24452), -- Ridge Stalker Patriarch
(2753,17259), -- Barnabus
(2850,23099), -- Broken Tooth
(2924,24603), -- Silvermane Wolf
(2925,24605), -- Silvermane Howler
(2926,23109), -- Silvermane Stalker
(2931,23148), -- Zaricotl
(2954,7371), -- Bristleback Battleboar
(2957,1742), -- Elder Plainstrider
(2958,17253), -- Prairie Wolf
(2959,17253), -- Prairie Stalker
(2960,17255), -- Prairie Wolf Alpha
(2960,24604), -- Prairie Wolf Alpha
(2966,7371), -- Battleboar
(3035,1742), -- Flatland Cougar
(3068,1742), -- Mazzranache
(3098,7371), -- Mottled Boar
(3099,7371), -- Dire Mottled Boar
(3100,7371), -- Elder Mottled Boar
(3106,16827), -- Pygmy Surf Crawler
(3108,16828), -- Encrusted Surf Crawler
(3110,17253), -- Dreadmaw Crocolisk
(3121,1742), -- Durotar Tiger
(3124,16827), -- Scorpid Worker
(3127,16828), -- Venomtail Scorpid
(3127,24640), -- Venomtail Scorpid
(3225,7371), -- Corrupted Mottled Boar
(3226,24640), -- Corrupted Scorpid
(3241,1753), -- Savannah Patriarch
(3245,1753), -- Ornery Plainstrider
(3246,1742), -- Fleeting Plainstrider
(3247,25008), -- Thunderhawk Hatchling
(3249,25008), -- Greater Thunderhawk
(3250,24640), -- Silithid Creeper
(3252,24640), -- Silithid Swarmer
(3281,16827), -- Sarkoth
(3424,25008), -- Thunderhawk Cloudscraper
(3461,17255), -- Oasis Snapjaw
(3472,25009), -- Washte Pawne
(3581,17260), -- Sewer Beast
(3619,16829), -- Ghost Saber
(3630,25008), -- Deviate Coiler
(3631,25008), -- Deviate Stinglash
(3653,17256), -- Kresh
(3653,26064), -- Kresh
(3809,16829), -- Ashenvale Bear
(3810,16830), -- Elder Ashenvale Bear
(3812,16829), -- Clattering Crawler
(3821,17257), -- Wildthorn Lurker
(3823,17256), -- Ghostpaw Runner
(3825,17257), -- Ghostpaw Alpha
(3825,24605), -- Ghostpaw Alpha
(4005,17256), -- Deepmoss Creeper
(4006,17256), -- Deepmoss Webspinner
(4067,1753), -- Twilight Runner
(4117,25009), -- Cloud Serpent
(4118,25009), -- Venomous Cloud Serpent
(4119,25009), -- Elder Cloud Serpent
(4126,1754), -- Crag Stalker
(4139,24583), -- Scorpid Terror
(4140,16830), -- Scorpid Reaver
(4140,24583), -- Scorpid Reaver
(4143,17258), -- Sparkleshell Snapper
(4158,24577), -- Salt Flats Vulture
(4304,23099), -- Scarlet Tracking Hound
(4341,17258), -- Drywallow Crocolisk
(4343,16831), -- Drywallow Snapper
(4344,17258), -- Mottled Drywallow Crocolisk
(4345,17259), -- Drywallow Daggermaw
(4346,23145), -- Noxious Flayer
(4347,23145), -- Noxious Reaver
(4347,25010), -- Noxious Reaver
(4348,23145), -- Noxious Shredder
(4348,25010), -- Noxious Shredder
(4396,17258), -- Mudrock Tortoise
(4400,17259), -- Mudrock Snapjaw
(4411,17258), -- Darkfang Lurker
(4412,17258), -- Darkfang Creeper
(4413,17258), -- Darkfang Spider
(4425,1754), -- Blind Hunter
(4425,23145), -- Blind Hunter
(4511,26178), -- Agam'ar
(4512,26178), -- Rotting Agam'ar
(4514,26178), -- Raging Agam'ar
(4538,1754), -- Kraul Bat
(4538,23145), -- Kraul Bat
(4539,23145), -- Greater Kraul Bat
(4662,23099), -- Magram Bonepaw
(4688,23099), -- Bonepaw Hyena
(4693,23145), -- Dread Flyer
(4694,24577), -- Dread Ripper
(4696,16830), -- Scorpashi Snapper
(4696,24583), -- Scorpashi Snapper
(4697,16831), -- Scorpashi Lasher
(4697,24583), -- Scorpashi Lasher
(4699,24583), -- Scorpashi Venomlash
(4821,16829), -- Skittering Crustacean
(4822,16829), -- Snapping Crustacean
(4823,16830), -- Barbed Crustacean
(4824,17256), -- Aku'mai Fisher
(4824,26064), -- Aku'mai Fisher
(4825,17257), -- Aku'mai Snapjaw
(4825,26064), -- Aku'mai Snapjaw
(4841,17259), -- Deadmire
(4861,1755), -- Shrike Bat
(4861,23145), -- Shrike Bat
(4861,24577), -- Shrike Bat
(4887,17257), -- Ghamoo-ra
(4887,26064), -- Ghamoo-ra
(4950,23099), -- Spot
(5053,17256), -- Deviate Crocolisk
(5056,25008), -- Deviate Dreadfang
(5262,26187), -- Groddoc Thunderer
(5268,16832), -- Ironfur Bear
(5274,3010), -- Ironfur Patriarch
(5286,17259), -- Longtooth Runner
(5286,23109), -- Longtooth Runner
(5286,24603), -- Longtooth Runner
(5287,24605), -- Longtooth Howler
(5291,25011), -- Hakkari Frostwing
(5307,23147), -- Vale Screecher
(5307,25010), -- Vale Screecher
(5308,23147), -- Rogue Vale Screecher
(5308,25010), -- Rogue Vale Screecher
(5349,23147), -- Arash-ethis
(5349,25011), -- Arash-ethis
(5352,16832), -- Old Grizzlegut
(5356,17259), -- Snarler
(5356,24603), -- Snarler
(5422,16832), -- Scorpid Hunter
(5422,24586), -- Scorpid Hunter
(5423,24586), -- Scorpid Tail Lasher
(5424,24586), -- Scorpid Dunestalker
(5425,23109), -- Starving Blisterpaw
(5426,23109), -- Blisterpaw Hyena
(5427,23109), -- Rabid Blisterpaw
(5428,23147), -- Roc
(5429,23147), -- Fire Roc
(5430,23147), -- Searing Roc
(5432,17260), -- Giant Surf Glider
(5432,26064), -- Giant Surf Glider
(5708,23148), -- Spawn of Hakkar
(5708,25011), -- Spawn of Hakkar
(5756,25008), -- Deviate Venomwing
(5766,1742), -- Savannah Cub
(5823,16828), -- Death Flayer
(5823,24640), -- Death Flayer
(5937,16831), -- Vile Sting
(5937,24583), -- Vile Sting
(5988,24586), -- Scorpok Stinger
(5992,26201), -- Ashmane Boar
(6013,23145), -- Wayward Buzzard
(6250,16828), -- Crawler
(6516,26188), -- Un'Goro Thunderer
(6585,17260), -- Uhk'loc
(6788,16829), -- Den Mother
(7022,16831), -- Venomlash Scorpid
(7055,23110), -- Blackrock Worg
(7078,24583), -- Cleft Scorpid
(7097,23147), -- Ironbeak Owl
(7097,24578), -- Ironbeak Owl
(7098,23148), -- Ironbeak Screecher
(7099,3010), -- Ironbeak Hunter
(7099,23148), -- Ironbeak Hunter
(7319,17255), -- Lady Sathrah
(7405,24586), -- Deadly Cleft Scorpid
(7430,16697), -- Frostsaber Cub
(7432,23110), -- Frostsaber Stalker
(7432,24453), -- Frostsaber Stalker
(7433,23110), -- Frostsaber Huntress
(7444,3010), -- Shardtooth Bear
(7445,3009), -- Elder Shardtooth
(7455,3010), -- Winterspring Owl
(7455,23148), -- Winterspring Owl
(7456,3009), -- Winterspring Screecher
(7456,23148), -- Winterspring Screecher
(7456,24579), -- Winterspring Screecher
(7803,24586), -- Scorpid Duneburrower
(8207,23147), -- Greater Firebird
(8208,23109), -- Murderous Blisterpaw
(8211,17259), -- Old Cliff Jumper
(8211,23109), -- Old Cliff Jumper
(8213,17260), -- Ironback
(8213,26064), -- Ironback
(8277,17260), -- Rekk'tilac
(8299,23148), -- Spiteflayer
(8300,23110), -- Ravage
(8301,3010), -- Clack the Reaver
(8301,24586), -- Clack the Reaver
(8303,23110), -- Grunter
(8303,26201), -- Grunter
(8336,25011), -- Hakkari Sapper
(8600,1756), -- Plaguebat
(8600,23148), -- Plaguebat
(8601,1756), -- Noxious Plaguebat
(8602,16697), -- Monstrous Plaguebat
(8602,24579), -- Monstrous Plaguebat
(8762,17259), -- Timberweb Recluse
(8926,3010), -- Deep Stinger
(8926,24586), -- Deep Stinger
(8927,1756), -- Dark Screecher
(8927,23148), -- Dark Screecher
(8927,24578), -- Dark Screecher
(8933,17260), -- Cave Creeper
(8958,3010), -- Angerclaw Mauler
(8959,17259), -- Felpaw Wolf
(8959,24603), -- Felpaw Wolf
(8961,17260), -- Felpaw Ravager
(9416,23110), -- Scarshield Worg
(9622,26188), -- U'cha
(9691,24586), -- Venomtip Scorpid
(9695,3010), -- Deathlash Scorpid
(9695,24586), -- Deathlash Scorpid
(9696,17261), -- Bloodaxe Worg
(9696,23110), -- Bloodaxe Worg
(9696,24597), -- Bloodaxe Worg
(9698,24587), -- Firetail Scorpid
(10200,23110), -- Rak'shiri
(10644,24604), -- Mist Howler
(11357,25012), -- Son of Hakkar
(11365,23110), -- Zulian Panther
(11370,23148), -- Razzashi Broodwidow
(11614,17260), -- Bloodshot
(11735,24586), -- Stonelash Scorpid
(11736,24587), -- Stonelash Pincer
(11737,24587), -- Stonelash Flayer
(11871,17255), -- Grinning Dog
(11871,24604), -- Grinning Dog
(11921,17256), -- Besseleth
(13599,17255), -- Stolid Snapjaw
(13896,26064), -- Scalebeard
(14223,17257), -- Cranky Benj
(14223,26064), -- Cranky Benj
(14233,17259), -- Ripscale
(14266,17256), -- Shanda the Spinner
(14274,24597), -- Winterax Tracker
(14279,17256), -- Creepthess
(14339,17259), -- Death Howl
(14339,24603), -- Death Howl
(14343,3010), -- Olm the Wise
(14343,23148), -- Olm the Wise
(14344,3010), -- Mongress
(14476,24587), -- Krellack
(15067,23110), -- Zulian Stalker
(15067,24453), -- Zulian Stalker
(15068,23110), -- Zulian Guardian
(15101,23110), -- Zulian Prowler
(15101,24453), -- Zulian Prowler
(16117,27685); -- Plagued Swine

-- Beast Training (5149) opens the window that teaches the hunter's ranks to the pet. It comes with Tame Beast and Call Pet, like
-- Revive Pet, Feed Pet and Dismiss Pet (2026_11_05_85); bots were already given it in code (PlayerbotAPI kHunterPetSpells).
DELETE FROM `spell_learn_spell` WHERE `entry` IN (1515, 883) AND `SpellID` = 5149;
INSERT INTO `spell_learn_spell` (`entry`, `SpellID`, `Active`) VALUES
(1515, 5149, 1),
(883, 5149, 1);
