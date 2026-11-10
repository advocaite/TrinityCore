-- Classic 1.60 (WoW Forever): quest givers and turn-ins for Forever quests the sniff import left without them.
--
-- Same method as Earthseer Farsen (2026_10_24_50). Sources, strongest first:
--   sniff quest POI: the 70124 sniff has accept/turn-in points for 20 of the quests; a spawned NPC or object at that point is it;
--   sniff RewardNextQuest: the NPC that takes quest A offers A's RewardNextQuest B, so B's giver = A's turn-in;
--   sniffed sibling quests (same text pattern, other race or class) that do have givers;
--   quest text: "return to X", "bring it to X", "speak with X" names the turn-in.
-- Rows marked INFERRED are not in any data: the giver is the speaker of the quest text, the area's adventurer or class trainer
-- with no quests of its own, or the turn-in of a "return to" quest. Replace them if a sniff shows another NPC.
-- Peacekeeper Vaaniel (252155) and Elegael Thornpaw (257944) had templates and quest rows but no spawn; they are spawned at the
-- sniffed POI points. Quests that still cannot be placed are listed in /mnt/project-files/missing-quests-247/README.md.
-- Also: npcflag gets the quest giver bit on every NPC used here (trainers and vendors lacked it), quests that are clearly
-- class quests get their class (they had none, so anyone could take them), and a few PrevQuestIDs follow the text.
-- Safe to run again.

-- quest givers
DELETE FROM `creature_queststarter` WHERE (`id`,`quest`) IN ((251361,92483),(251361,92484),(251361,92532),(251374,92466),(251374,92467),(252382,97243),(268592,97244),(268592,97245),(268592,97257),(254151,93459),(252377,94003),(263664,97973),(265813,96057),(263399,96626),(263399,97925),(263399,97923),(265812,97960),(240,91777),(383,99129),(241,99130),(241,99131),(251507,94792),(251507,94863),(251507,94864),(251507,94793),(1261,95212),(1256,95214),(271546,98323),(658,98574),(3033,98340),(11802,98405),(11957,98342),(11957,98739),(11802,98362),(246378,94427),(257648,94434),(257648,94435),(246378,94436),(257655,94441),(11860,86576),(267009,96897),(267009,96898),(10665,99142),(1951,91920),(1937,91921),(2121,95981),(10266,96874),(264074,96241),(2988,98427),(2948,98435),(3222,99079),(275789,99081),(3222,99101),(2993,99082),(276171,99196),(276170,99191),(341,98386),(1070,98407),(167,86776),(3432,95774));
INSERT INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES
(251361,92483,0), -- At Home in the Shadows: Rorian the Dayseeker INFERRED. Rorian the Dayseeker, like the sniffed sibling letters 92482 and 92485 ("while you were away dealing with our vuldren problem" = 92461)
(251361,92484,0), -- Embracing the Elements: Rorian the Dayseeker INFERRED. Rorian the Dayseeker, like the sniffed sibling letters 92482 and 92485 ("while you were away dealing with our vuldren problem" = 92461)
(251361,92532,0), -- The Warrior's Path: Rorian the Dayseeker INFERRED. Rorian the Dayseeker, like the sniffed sibling letters 92482 and 92485 ("while you were away dealing with our vuldren problem" = 92461)
(251374,92466,0), -- Call of Earth: Windshaper Boro INFERRED. quest text: bring me one of their signets / bring a Signet of Akir to Windshaper Boro
(251374,92467,0), -- Call of Earth: Windshaper Boro. sniff RewardNextQuest of the previous quest, whose turn-in is this NPC (92466)
(252382,97243,0), -- Call of Fire: Sessaria Skystride INFERRED. Sessaria Skystride, the Valanaar shaman trainer: the text sends you from Valanaar to the Shrine of Flames west of it
(268592,97244,0), -- Call of Fire: Olariaan Swiftburn INFERRED. quest text: the speaker needs the heart for "the ritual we are to perform" (Olariaan)
(268592,97245,0), -- Call of Fire: Olariaan Swiftburn INFERRED. quest text: bring Kuramaa's Mask to Olariaan; the speaker asks for it
(268592,97257,0), -- Call of Fire: Olariaan Swiftburn. sniff RewardNextQuest of the previous quest, whose turn-in is this NPC (97245)
(254151,93459,0), -- More Al'Aketh Ears: Vayn Moongaze INFERRED. Vayn Moongaze gives the first ears quest 93165 with the same objective; the text calls him Vayn Moonclaw, REQ "<Vayn nods at you.>"
(252377,94003,0), -- The Skybreaker Bulwark: Seena Skybreaker INFERRED. Seena Skybreaker, warrior trainer in Valanaar with no quests: "I am all that remains of the line of Skybreakers ... I will teach you the Skybreaker Defense"
(263664,97973,0), -- Camping 101: Tailoring: Raan Wildwind INFERRED. Raan Wildwind, like the sniffed horde twin 97972 (same text and turn-in)
(265813,96057,0), -- Camping 101: Tailoring: Eric Brighthammer INFERRED. Eric Brighthammer, the Kharanos adventurer (each start area's adventurer gives its Camping 101 quests, see Eleanor Shackleton in Brill)
(263399,96626,0), -- Camping 101: Cooking: Sam Sarsaparilla INFERRED. Sam Sarsaparilla, the Goldshire adventurer (Raan and Eleanor give their area's Camping 101: Cooking)
(263399,97925,0), -- Camping 101: Tailoring: Sam Sarsaparilla INFERRED. Sam Sarsaparilla, the Goldshire adventurer
(263399,97923,0), -- Camping 101: Mining: Sam Sarsaparilla INFERRED. Sam Sarsaparilla, the Goldshire adventurer
(265812,97960,0), -- Camping 101: Skinning: Eleanor Shackleton INFERRED. Eleanor Shackleton, the Brill adventurer who gives all other Brill Camping 101 quests
(240,91777,0), -- Rare Books: Marshal Dughan. sniff RewardNextQuest of the previous quest, whose turn-in is this NPC (91775, Marshal Dughan)
(383,99129,0), -- A Man About a Murloc: Jason Mathers INFERRED. quest text: "my fish", Jason's murloc problems; Jason Mathers gives 99127/99128
(241,99130,0), -- An Enticing Offer: Remy "Two Times" INFERRED. speaker is Remy ("truly a win-win") after 99129
(241,99131,0), -- Baited for Success: Remy "Two Times" INFERRED. speaker is Remy: "Tell Mathers he can come pick up the bait"
(251507,94792,0), -- Taming the Beast: Josephine Carson INFERRED. Josephine Carson, Goldshire hunter trainer (the turn-in)
(251507,94863,0), -- Taming the Beast: Josephine Carson. sniff RewardNextQuest of the previous quest, whose turn-in is this NPC (94792)
(251507,94864,0), -- Taming the Beast: Josephine Carson. sniff RewardNextQuest of the previous quest, whose turn-in is this NPC (94863)
(251507,94793,0), -- Training the Beast: Josephine Carson. sniff RewardNextQuest of the previous quest, whose turn-in is this NPC (94864)
(1261,95212,0), -- Never Saddle on Quality: Veron Amberstill INFERRED. Veron Amberstill <Ram Breeder>: "send a team of rams ... material to make their saddles", deliver to Amberstill Ranch
(1256,95214,0), -- Stolen Blasting Powder: Quarrymaster Thesten INFERRED. speaker got the keg from 95213
(271546,98323,0), -- Secure the Mountain: Mountaineer Gretchen. provided item is Mountaineer Gretchen's Report; follows her 98319
(658,98574,0), -- Hallowed Memorandum: Sten Stoutarm INFERRED. Sten Stoutarm, like the sniffed sibling letter 98581 (after 179)
(3033,98340,0), -- The Great Cat Spirit: Turak Runetotem INFERRED. Turak Runetotem: 98362 sends you back to him as "the one who sent you to me"
(11802,98405,0), -- The Great Cat Spirit: Dendrite Starblaze INFERRED. Dendrite Starblaze, as for the Great Bear Spirit (5929); tauren follow-up of 98340
(11957,98342,0), -- The Great Cat Spirit: Great Cat Spirit. sniff RewardNextQuest of the previous quest, whose turn-in is this NPC (98405)
(11957,98739,0), -- Blessings of the Great Cat Spirit: Great Cat Spirit. sniff RewardNextQuest of the previous quest, whose turn-in is this NPC (98342)
(11802,98362,0), -- To Thunder Bluff: Dendrite Starblaze. sniff RewardNextQuest of the previous quest, whose turn-in is this NPC (98739)
(246378,94427,0), -- A Lesson in Divinity: Danitha Morr INFERRED. Danitha Morr: 94435 and 94441 send you back to her
(257648,94434,0), -- A Lesson in Divinity: Tanis Alderwood INFERRED. speaker is Tanis ("my wife")
(257648,94435,0), -- A Lesson in Divinity: Tanis Alderwood. sniff RewardNextQuest of the previous quest, whose turn-in is this NPC (94434)
(246378,94436,0), -- A Lesson in Divinity: Danitha Morr. sniff RewardNextQuest of the previous quest, whose turn-in is this NPC (94435)
(257655,94441,0), -- A Lesson in Divinity: Deathguard Billmuth. sniff RewardNextQuest of the previous quest, whose turn-in is this NPC (94440)
(11860,86576,0), -- Bloodfury Trinkets: Maggran Earthbinder. sniff quest POI: accept point 0.7 yd from Maggran Earthbinder
(267009,96897,0), -- The Cult of the Damned: Hadric Harlson INFERRED. speaker is the Argent emissary at the cultist site (Hadric Harlson)
(267009,96898,0), -- Remnants of War: Hadric Harlson INFERRED. speaker is the Argent emissary at the cultist site (Hadric Harlson)
(10665,99142,0), -- Tomb Weed: Junior Apothecary Holland INFERRED. speaker grows doom weed and mentions Gordo (Holland's 5481/5482)
(1951,91920,0), -- Wild Eyes: Quinn Yorick INFERRED. speaker drank the apothecary's potion and is the patient of "Return to Quinn" (430): Quinn Yorick
(1937,91921,0), -- Return to Quinn (Again): Apothecary Renferrel. sniff RewardNextQuest of the previous quest, whose turn-in is this NPC (91920)
(2121,95981,0), -- Watching the Roads: Shadow Priest Allister INFERRED. the turn-in also gives the follow-up 480 named by RewardNextQuest
(10266,96874,0), -- This Is Spinal Axe: Ug'thok INFERRED. speaker's own task, "Ug'thok wants you to"
(264074,96241,0), -- Talkin' bout Toxins, Mon: Mazu'kon. speaker calls himself "Ol' Mazu'kon"
(2988,98427,0), -- Ceasing Operations: Morin Cloudstalker. sniff RewardNextQuest of the previous quest, whose turn-in is this NPC (98424)
(2948,98435,0), -- Thunderhorn's Report: Mull Thunderhorn INFERRED. item name "Thunderhorn's Report": Mull Thunderhorn of Bloodhoof Village
(3222,99079,0), -- Longwalker Malah: Brave Wildrunner INFERRED. Brave Wildrunner, who gets the report back in 99081, guards Bloodhoof ("I must ensure the village remains safe")
(275789,99081,0), -- Grim Tidings: Malah Longwind. sniff RewardNextQuest of the previous quest, whose turn-in is this NPC (99079)
(3222,99101,0), -- Our Ancient Enemy: Brave Wildrunner. sniff RewardNextQuest of the previous quest, whose turn-in is this NPC (99081)
(2993,99082,0), -- The High Chieftain: Baine Bloodhoof INFERRED. speaker sends you to "my father"; follows Baine's 99080
(276171,99196,0), -- A Donation of Wool: Oura Stormspinner INFERRED. Oura Stormspinner <Windshapers Cloth Quartermaster>: the quest gives Windshapers reputation (2778)
(276170,99191,0), -- A Donation of Wool: Belanaa Windveil INFERRED. Belanaa Windveil <High Order Cloth Quartermaster> by Dalaran: the quest gives High Order reputation (2779), zone Dalaran
(341,98386,0), -- Alther's Mill: Foreman Oslow INFERRED. speaker builds the bridge (Foreman Oslow)
(1070,98407,0), -- Show of Force: Deputy Feldon INFERRED. speaker's own task near Lakeshire
(167,86776,0), -- Ingredients for the Forge: Morhan Coppertongue INFERRED. speaker lives in Thelsamar and needs bronze
(3432,95774,0); -- Her Name Is Olgra: Mankrik INFERRED. speaker builds Olgra's memorial (Mankrik, 4921 Lost in Battle)

-- quest turn-ins
DELETE FROM `creature_questender` WHERE (`id`,`quest`) IN ((251389,92483),(251374,92484),(251964,92532),(251374,92466),(251374,92468),(268592,97243),(268592,97244),(268592,97245),(252382,97257),(254151,93459),(252377,94003),(251991,97973),(1703,96057),(1430,96626),(1103,97925),(5513,97923),(6289,97960),(2855,96102),(5938,97932),(11051,97937),(11050,97950),(951,91777),(241,99129),(241,99130),(383,99131),(251507,94792),(251507,94863),(251507,94864),(258930,94793),(1261,95212),(1256,95213),(1256,95214),(1252,98323),(837,98574),(11802,98340),(11957,98405),(11957,98342),(11802,98739),(3033,98362),(257648,94427),(257648,94434),(246378,94435),(257655,94436),(257655,94440),(246378,94441),(376,94774),(267009,96895),(267009,96897),(267009,96898),(10665,99142),(1937,91920),(1951,91921),(2121,95981),(10266,96874),(264074,96241),(2988,98427),(2988,98424),(5769,98435),(275789,99079),(3222,99081),(2993,99101),(3057,99082),(259118,98823),(276171,99196),(276170,99191),(341,98386),(1070,98407),(167,86776),(3432,95774));
INSERT INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES
(251389,92483,0), -- At Home in the Shadows: Akeri Duskblade. quest text: speak with Akeri Duskblade
(251374,92484,0), -- Embracing the Elements: Windshaper Boro. quest text: speak with Windshaper Boro
(251964,92532,0), -- The Warrior's Path: Blademaster Ren. quest text: speak with Blademaster Ren
(251374,92466,0), -- Call of Earth: Windshaper Boro. quest text: bring a Signet of Akir to Windshaper Boro
(251374,92468,0), -- Call of Earth: Windshaper Boro. quest text: bring the Rough Quartz to Windshaper Boro(s)
(268592,97243,0), -- Call of Fire: Olariaan Swiftburn. quest text: bring the torch to Olariaan Swiftburn at the Shrine of Flames
(268592,97244,0), -- Call of Fire: Olariaan Swiftburn INFERRED. quest text: bring me his heart
(268592,97245,0), -- Call of Fire: Olariaan Swiftburn. quest text: bring Kuramaa's Mask to Olariaan Swiftburn
(252382,97257,0), -- Call of Fire: Sessaria Skystride INFERRED. quest text: place the flame in the Brazier of Eternal Flames in Valanaar; Sessaria stands 13 yards from that brazier
(254151,93459,0), -- More Al'Aketh Ears: Vayn Moongaze INFERRED. quest text: return them to Vayn (Moonclaw) in Shadowgale Forest
(252377,94003,0), -- The Skybreaker Bulwark: Seena Skybreaker INFERRED. same speaker (reclaim my shield)
(251991,97973,0), -- Camping 101: Tailoring: Taleen Shimmerthread. quest text: speak with Taleen Shimmerthread; same as 97972
(1703,96057,0), -- Camping 101: Tailoring: Uthrar Threx. quest text: speak with Uthrar Threx in Ironforge
(1430,96626,0), -- Camping 101: Cooking: Tomas. quest text: speak with Tomas in Goldshire
(1103,97925,0), -- Camping 101: Tailoring: Eldrin. quest text: speak with Eldrin in Eastvale Logging Camp
(5513,97923,0), -- Camping 101: Mining: Gelman Stonehand. quest text: speak with Gelman Stonehand in Stormwind
(6289,97960,0), -- Camping 101: Skinning: Rand Rhobart. quest text: speak with Rand Rhobart on the road to Undercity
(2855,96102,0), -- Camping 101: Tailoring: Snang. quest text: speak with Snang in Orgrimmar
(5938,97932,0), -- Camping 101: Fishing: Uthan Stillwater. quest text: speak with Uthan Stillwater in Bloodhoof Village
(11051,97937,0), -- Camping 101: Tailoring: Vhan. quest text: speak with Vhan in Thunder Bluff
(11050,97950,0), -- Camping 101: Tailoring: Trianna. quest text: speak with Trianna in Darnassus
(951,91777,0), -- Rare Books: Brother Paxton. quest text: return them to Brother Paxton
(241,99129,0), -- A Man About a Murloc: Remy "Two Times". quest text: speak with Remy "Two Times"
(241,99130,0), -- An Enticing Offer: Remy "Two Times" INFERRED. same speaker
(383,99131,0), -- Baited for Success: Jason Mathers. quest text: return to Jason Mathers
(251507,94792,0), -- Taming the Beast: Josephine Carson. quest text: return the Taming Rod to Josephine Carson
(251507,94863,0), -- Taming the Beast: Josephine Carson. quest text: return the Taming Rod to Josephine Carson
(251507,94864,0), -- Taming the Beast: Josephine Carson. quest text: return the Taming Rod to Josephine Carson
(258930,94793,0), -- Training the Beast: Isaac Chan. quest text: speak to Isaac Chan in Goldshire
(1261,95212,0), -- Never Saddle on Quality: Veron Amberstill INFERRED. same
(1256,95213,0), -- Stolen Blasting Powder: Quarrymaster Thesten. quest text: bring the keg to Quarrymaster Thesten
(1256,95214,0), -- Stolen Blasting Powder: Quarrymaster Thesten. quest text: return to Quarrymaster Thesten
(1252,98323,0), -- Secure the Mountain: Senir Whitebeard. quest text: deliver the report to Senir Whitebeard
(837,98574,0), -- Hallowed Memorandum: Branstock Khalder. quest text: speak to Branstock Khalder in Anvilmar
(11802,98340,0), -- The Great Cat Spirit: Dendrite Starblaze. quest text: speak with Dendrite Starblaze
(11957,98405,0), -- The Great Cat Spirit: Great Cat Spirit. quest text: seek out the Great Cat Spirit
(11957,98342,0), -- The Great Cat Spirit: Great Cat Spirit. quest text: return them to the Great Cat Spirit
(11802,98739,0), -- Blessings of the Great Cat Spirit: Dendrite Starblaze. quest text: speak to Dendrite
(3033,98362,0), -- To Thunder Bluff: Turak Runetotem. quest text: speak with Turak Runetotem on the Elder Rise
(257648,94427,0), -- A Lesson in Divinity: Tanis Alderwood. quest text: speak to Tanis Alderwood in the Undercity
(257648,94434,0), -- A Lesson in Divinity: Tanis Alderwood. quest text: bring 10 Linen Cloth to Tanis Alderwood
(246378,94435,0), -- A Lesson in Divinity: Danitha Morr. quest text: return to Danitha Morr
(257655,94436,0), -- A Lesson in Divinity: Deathguard Billmuth. quest text: speak with Deathguard Billmuth
(257655,94440,0), -- A Lesson in Divinity: Deathguard Billmuth. quest text: return to Deathguard Billmuth
(246378,94441,0), -- A Lesson in Divinity: Danitha Morr. quest text: speak with Danitha Morr
(376,94774,0), -- Divine Grace: High Priestess Laurena. quest text: speak to High Priestess Laurena
(267009,96895,0), -- The Argent Emissary: Hadric Harlson INFERRED. Hadric Harlson <The Argent Dawn>, the only Argent Dawn NPC sniffed in Tirisfal, southeast of the Undercity entrance as the text says
(267009,96897,0), -- The Cult of the Damned: Hadric Harlson INFERRED. same
(267009,96898,0), -- Remnants of War: Hadric Harlson INFERRED. same
(10665,99142,0), -- Tomb Weed: Junior Apothecary Holland. quest text: deliver them back to Junior Apothecary Holland
(1937,91920,0), -- Wild Eyes: Apothecary Renferrel. quest text: bring them to Apothecary Renferrel
(1951,91921,0), -- Return to Quinn (Again): Quinn Yorick. quest text: bring the potion to Quinn Yorick
(2121,95981,0), -- Watching the Roads: Shadow Priest Allister. quest text: for Shadow Priest Allister
(10266,96874,0), -- This Is Spinal Axe: Ug'thok. quest text: Ug'thok in Orgrimmar
(264074,96241,0), -- Talkin' bout Toxins, Mon: Mazu'kon INFERRED. same speaker (take my extractor)
(2988,98427,0), -- Ceasing Operations: Morin Cloudstalker. quest text: bring the key to Morin Cloudstalker
(2988,98424,0), -- Fizsprocket's Notes: Morin Cloudstalker. quest text: bring them to Morin Cloudstalker
(5769,98435,0), -- Thunderhorn's Report: Arch Druid Hamuul Runetotem. quest text: bring it to Arch Druid Hamuul Runetotem
(275789,99079,0), -- Longwalker Malah: Malah Longwind. quest text: find Longwalker Malah Longwind
(3222,99081,0), -- Grim Tidings: Brave Wildrunner. quest text: bring the report to Brave Wildrunner
(2993,99101,0), -- Our Ancient Enemy: Baine Bloodhoof. quest text: bring the report to Baine Bloodhoof
(3057,99082,0), -- The High Chieftain: Cairne Bloodhoof. quest text: seek audience with Cairne Bloodhoof
(259118,98823,0), -- Earthen Echo: Muln Earthfury. quest text: bring the relic to Muln Earthfury at the Skywatcher Plateau
(276171,99196,0), -- A Donation of Wool: Oura Stormspinner INFERRED. same
(276170,99191,0), -- A Donation of Wool: Belanaa Windveil INFERRED. same
(341,98386,0), -- Alther's Mill: Foreman Oslow. quest text: for Foreman Oslow
(1070,98407,0), -- Show of Force: Deputy Feldon. quest text: for Deputy Feldon
(167,86776,0), -- Ingredients for the Forge: Morhan Coppertongue. quest text: bring bronze bars to Morhan Coppertongue
(3432,95774,0); -- Her Name Is Olgra: Mankrik. quest text: for Mankrik's memorial

-- quest turn-ins (objects)
DELETE FROM `gameobject_questender` WHERE (`id`,`quest`) IN ((581822,92727));
INSERT INTO `gameobject_questender` (`id`,`quest`,`VerifiedBuild`) VALUES
(581822,92727,0); -- The Missing Scholar: Bloodstained Satchel. sniff quest POI: turn-in point 0.4 yd from the Bloodstained Satchel

-- quest giver flag (2) on NPCs that lacked it: 167 Morhan Coppertongue, 1103 Eldrin, 1430 Tomas, 3222 Brave Wildrunner, 5513 Gelman Stonehand, 6289 Rand Rhobart, 10266 Ug'thok, 251374 Windshaper Boro, 251376 Tai'ree Farsight, 251389 Akeri Duskblade, 251964 Blademaster Ren, 276170 Belanaa Windveil
-- (251376 Tai'ree Farsight ends the sniffed 92482 The Way of the Hunter and had no quest giver flag either)
UPDATE `creature_template` SET `npcflag` = `npcflag` | 2 WHERE `entry` IN (167,1103,1430,3222,5513,6289,10266,251374,251376,251389,251964,276170);
UPDATE `creature` SET `npcflag` = `npcflag` | 2 WHERE `id` IN (167,1103,1430,3222,5513,6289,10266,251374,251376,251389,251964,276170) AND `npcflag` IS NOT NULL;

-- class quests without a class (AllowableClasses was empty)
INSERT INTO `quest_template_addon` (`ID`,`AllowableClasses`) VALUES
(92466,64), -- Call of Earth
(92483,8), -- At Home in the Shadows
(92484,64), -- Embracing the Elements
(92532,1), -- The Warrior's Path
(94003,1), -- The Skybreaker Bulwark
(94427,2), -- A Lesson in Divinity
(94434,2), -- A Lesson in Divinity
(94435,2), -- A Lesson in Divinity
(94436,2), -- A Lesson in Divinity
(94438,2), -- A Lesson in Divinity
(94440,2), -- A Lesson in Divinity
(94441,2), -- A Lesson in Divinity
(94792,4), -- Taming the Beast
(94793,4), -- Training the Beast
(94863,4), -- Taming the Beast
(94864,4), -- Taming the Beast
(97243,64), -- Call of Fire
(97244,64), -- Call of Fire
(97245,64), -- Call of Fire
(97257,64), -- Call of Fire
(98340,1024), -- The Great Cat Spirit
(98341,1024), -- The Great Windborne Cat Spirit
(98342,1024), -- The Great Cat Spirit
(98362,1024), -- To Thunder Bluff
(98404,1024), -- The Great Windborne Cat Spirit
(98405,1024), -- The Great Cat Spirit
(98574,16), -- Hallowed Memorandum
(98738,1024), -- Blessings of the Great Windborne Cat Spirit
(98739,1024) -- Blessings of the Great Cat Spirit
ON DUPLICATE KEY UPDATE `AllowableClasses` = VALUES(`AllowableClasses`);

-- previous quests
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES
(92483,92461), -- At Home in the Shadows after 92461 INFERRED: same as the sibling letters 92482 and 92485
(92484,92461), -- Embracing the Elements after 92461 INFERRED: same as the sibling letters 92482 and 92485
(92532,92461), -- The Warrior's Path after 92461 INFERRED: same as the sibling letters 92482 and 92485
(93459,93165), -- More Al'Aketh Ears after 93165 INFERRED: "More" ears after Mercy Falls on Deaf Ears
(94434,94427), -- A Lesson in Divinity after 94427 INFERRED: text order
(95214,95213), -- Stolen Blasting Powder after 95213 INFERRED: the keg quest leads into it
(97244,97243), -- Call of Fire after 97243 INFERRED: the torch from 97243 is the start of the ritual
(97245,97243), -- Call of Fire after 97243 INFERRED: the torch from 97243 is the start of the ritual
(97923,95998), -- Camping 101: Mining after 95998 INFERRED: Sam's The Great Outdoors; the import had Raan's 96101 on Zephras, which an Elwynn player never gets
(97973,96101), -- Camping 101: Tailoring after 96101 INFERRED: same as the twin 97972
(98405,98340), -- The Great Cat Spirit after 98340 INFERRED: the windborne twin 98341 follows 98340
(98574,179), -- Hallowed Memorandum after 179 INFERRED: same as the sibling 98581
(99130,99129), -- An Enticing Offer after 99129 INFERRED: text order
(99131,99130), -- Baited for Success after 99130 INFERRED: text order
(99142,5482) -- Tomb Weed after 5482 INFERRED: Doom Weed
ON DUPLICATE KEY UPDATE `PrevQuestID` = VALUES(`PrevQuestID`);

-- spawns: Peacekeeper Vaaniel (dead, The Western Watch / A Last Request) and Elegael Thornpaw (Unnerving Silence chain), Zephras
-- 252155 Peacekeeper Vaaniel: turn-in point of 93926 and start point of 93927 (2658 1876 773 / 2655 1875 773), 2.6 yd from the Bloody Note (gameobject 617704); a corpse like the Dead Cultist (aura 29266); z from the POI and the nearby objects, facing not in the data
-- 257944 Elegael Thornpaw: turn-in/start points of 94484-94491 (3494..3506, 794..808, 801); z from the POI and the nearby objects, facing not in the data
DELETE FROM `creature` WHERE `guid` IN (25215500,25794400) OR `id` IN (252155,257944);
INSERT INTO `creature` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`position_x`,`position_y`,`position_z`,`orientation`,`modelid`,`equipment_id`,`spawntimesecs`,`wander_distance`,`currentwaypoint`,`curHealthPct`,`MovementType`,`npcflag`,`unit_flags`,`unit_flags2`,`unit_flags3`,`ScriptName`,`VerifiedBuild`) VALUES
(25215500,252155,2991,0,0,'0',0,0,0,-1,2657.0,1875.5,772.8,3.59,0,0,300,0,0,100,0,NULL,NULL,NULL,NULL,'',0),
(25794400,257944,2991,0,0,'0',0,0,0,-1,3503.0,797.0,800.6,0.0,0,0,300,0,0,100,0,NULL,NULL,NULL,NULL,'',0);
-- Vaaniel lies dead like the sniffed Dead Cultist (253372, aura 29266 Permanent Feign Death); Elegael keeps her sniffed addon
DELETE FROM `creature_template_addon` WHERE `entry` = 252155;
INSERT INTO `creature_template_addon` (`entry`,`PathId`,`mount`,`MountCreatureID`,`StandState`,`AnimTier`,`VisFlags`,`SheathState`,`PvPFlags`,`emote`,`aiAnimKit`,`movementAnimKit`,`meleeAnimKit`,`visibilityDistanceType`,`auras`) VALUES
(252155,0,0,0,0,0,0,1,0,0,0,0,0,0,'29266');
-- levels: no sniff level; Vaaniel as the Peacekeepers of Zephras (253474, 10), Elegael as the druids of her den (Jorel Windsinger 258130, 10)
INSERT IGNORE INTO `creature_classic_level` (`entry`,`level_min`,`level_max`) VALUES (252155,10,10),(257944,10,10);
