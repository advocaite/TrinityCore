-- Classic 1.60: vanilla quest prerequisites the retail quest_template_addon rows dropped (VMaNGOS 1.12 PrevQuestId).
-- Without them these quests are offered before the quest that leads to them. Only plain chains are restored: the previous quest
-- exists, has a spawned starter, is not a breadcrumb and is not one of several exclusive routes. A negative id means the previous
-- quest has to be active (VMaNGOS and TrinityCore read it the same way).
INSERT INTO `quest_template_addon` (`ID`, `PrevQuestID`) VALUES
(5,163), -- Jitters' Growling Gut <- Raven Hill
(11,239), -- Riverpaw Gnoll Bounty <- Westbrook Garrison Needs Help!
(148,165), -- Supplies from Darkshire <- The Hermit
(353,1097), -- Stormpike's Delivery <- Elmore's Task
(364,363), -- The Mindless Ones <- Rude Awakening
(429,428), -- Wild Hearts <- Lost Deathstalkers
(466,467), -- Search for Incendicite <- Stonegear's Search
(484,469), -- Young Crocolisk Skins <- Daily Delivery
(518,495), -- The Crown of Will <- The Crown of Will
(639,638), -- Sigil of Strom <- Trollbane
(738,707), -- Find Agmond <- Ironband Wants You!
(769,768), -- Kodo Hide Bag <- Gathering Leather
(918,997), -- Timberling Seeds <- Denalan's Earth
(1011,4581), -- Forsaken Diseases <- Kayneth Stillwind
(1085,1070), -- On Guard in Stonetalon <- On Guard in Stonetalon
(1131,1130), -- Steelsnap <- Melor Sends Word
(1204,1260), -- Mudrock Soup and Bugs <- Morgan Stern
(1275,3765), -- Researching the Corruption <- The Corruption Abroad
(1302,1301), -- James Hyal <- James Hyal
(1395,1477), -- Supplies for Nethergarde <- Vital Supplies
(1642,1641), -- The Tome of Divinity <- The Tome of Divinity
(1646,1645), -- The Tome of Divinity <- The Tome of Divinity
(1758,1798), -- Tome of the Cabal <- Seeking Strahad
(1824,1823), -- Trial at the Field of Giants <- Speak with Ruga
(1920,1919), -- Investigate the Blue Recluse <- Report to Jennea
(1938,1939), -- Ur's Treatise on Shadow Magic <- High Sorcerer Andromath
(1944,1943), -- Waters of Xavian <- Speak with Deino
(1960,1959), -- Investigate the Alchemist Shop <- Report to Anastasia
(2240,2398), -- The Hidden Chamber <- The Lost Dwarves
(2518,2519), -- Tears of the Moon <- The Temple of the Moon
(2851,2847), -- Wild Leather Boots <- Wild Leather Armor
(2852,2847), -- Wild Leather Leggings <- Wild Leather Armor
(2858,2854), -- Wild Leather Boots <- Wild Leather Armor
(2859,2854), -- Wild Leather Leggings <- Wild Leather Armor
(2865,2864), -- Scarab Shells <- Tran'rek
(2922,2923), -- Save Techbot's Brain! <- Tinkmaster Overspark
(4022,3481), -- A Taste of Flame <- Trinkets...
(4126,4128), -- Hurley Blackbreath <- Ragnar Thunderbrew
(4134,4133), -- Lost Thunderbrew Recipe <- Vivian Lagrave
(4136,4324), -- Ribbly Screwspigot <- Yuka Screwspigot
(4764,4766), -- Doomrigger's Clasp <- Mayara Brightwing
(4768,4769), -- The Darkstone Tablet <- Vivian Lagrave and the Darkstone Tablet
(4962,-1799), -- Shard of a Felhound <- Fragments of the Orb of Orahil
(4963,-1799), -- Shard of an Infernal <- Fragments of the Orb of Orahil
(5082,6603), -- Threat of the Winterfall <- Trouble in Winterspring!
(5621,5622), -- Garments of the Moon <- In Favor of Elune
(5624,5623), -- Garments of the Light <- In Favor of the Light
(5625,5626), -- Garments of the Light <- In Favor of the Light
(5648,5649), -- Garments of Spirituality <- In Favor of Spirituality
(5650,5651), -- Garments of Darkness <- In Favor of Darkness
(7641,7640), -- The Work of Grimand Elmore <- Exorcising Terrordale
(7643,7642), -- Ancient Equine Spirit <- Collection of Goods
(8414,8415), -- Dispelling Evil <- Chillwind Point
(9052,9063) -- Bloodpetal Poison <- Torwa Pathfinder
ON DUPLICATE KEY UPDATE `PrevQuestID` = IF(`PrevQuestID` = 0, VALUES(`PrevQuestID`), `PrevQuestID`);
