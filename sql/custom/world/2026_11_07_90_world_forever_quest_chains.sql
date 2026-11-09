-- Quest chains: Forever quests could all be taken at once because the quest before them was never required.
-- Player report 2026-11-07 (#общий, alexia5746): in the Westfall Moonbrook/Deadmines chain 92745-92753 every quest can be
-- taken without the one before, and 92750 Detonation at a Distance too. The restore of Forever quest data (2026_11_05_99)
-- brought these quests back from data that never had their chain links, so PrevQuestID stayed 0 and no condition was set.
-- Checked every quest 80000-99999 that has a giver here against the Questie project's Forever quest data
-- (github.com/Questie/QuestieDB, src/corrections/Forever and data/Forever, GPLv3, chains recorded from the live game),
-- and, where Questie has no chain for a quest, against Wowhead quest series and the quest texts. Real starters stay open.
-- Only quests with no chain link at all get one; existing links are left as they are, except Tears of the Lady, Feathers
-- for Binding and Unwanted and Unworthy, which now also accept A Sacrifice in Vain (the other way into the same story).
-- One previous quest: PrevQuestID. All of several: conditions in one group. Any one of several: conditions in separate groups.
-- Safe to run again.

-- 1) One previous quest, from Questie's Forever data
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES
(91208,90902),
(91316,91294),
(91724,91723),
(92422,99156),
(92468,92467),
(92597,92461),
(92698,92679),
(92741,92699),
(92747,92745),
(92748,92747),
(92749,92748),
(92750,92748),
(92751,92750),
(92753,92752),
(92911,92910),
(93736,92700),
(94473,94466),
(95216,451),
(95803,91317),
(95805,755),
(95981,482),
(96630,921),
(96652,794),
(96659,757),
(96897,96895),
(96898,96895),
(97977,458),
(97979,456),
(98046,935),
(98065,98046),
(98386,89),
(98435,760),
(99049,99048),
(99051,99049),
(99052,99051),
(99073,99050),
(99129,99128),
(99152,96896),
(99153,96896),
(99160,99159),
(99161,99160),
(99162,99159)
ON DUPLICATE KEY UPDATE `PrevQuestID` = VALUES(`PrevQuestID`);
--   91208 Coming to Terms  <-  90902 Rediscovering the Light
--   91316 Making Repairs  <-  91294 Touring the Grounds
--   91724 Delicate Instruments  <-  91723 Delicate Instruments
--   92422 The Wrath of Rath'mael  <-  99156 Rear Guard Patrol
--   92468 Call of Earth  <-  92467 Call of Earth
--   92597 Reading the Ley Lines  <-  92461 Harmony in Balance
--   92698 What Is My Purpose?  <-  92679 Blood Tithe
--   92741 Unwelcome Visitors  <-  92699 The Supreme Magister
--   92747 Moonbrook Espionage  <-  92745 The State of the Mines
--   92748 Explosive Consultation  <-  92747 Moonbrook Espionage
--   92749 A Dynamite Plan  <-  92748 Explosive Consultation
--   92750 Detonation at a Distance  <-  92748 Explosive Consultation
--   92751 Detonation at a Distance  <-  92750 Detonation at a Distance
--   92753 Destruction in Deadmines  <-  92752 Explosive Consultation
--   92911 Harvesting the Harvesters  <-  92910 Harvesting the Harvesters
--   93736 Unwelcome Spirits  <-  92700 The Grand Skyseer
--   94473 Fire Sapta  <-  94466 Call of Fire
--   95216 The New Plague  <-  451 A Recipe For Death
--   95803 A Token of Good Faith  <-  91317 The Tarnished
--   95805 Grace of An'she and Mu'sha  <-  755 Rites of the Earthmother
--   95981 Watching the Roads  <-  482 Dalaran's Intentions
--   96630 The Adventurer  <-  921 Crown of the Earth
--   96652 The Adventurer  <-  794 Burning Blade Medallion
--   96659 The Adventurer  <-  757 Rite of Strength
--   96897 The Cult of the Damned  <-  96895 The Argent Emissary
--   96898 Remnants of War  <-  96895 The Argent Emissary
--   97977 Nature's Call  <-  458 The Woodland Protector
--   97979 The Goddess Provides  <-  456 The Balance of Nature
--   98046 Crown of the Earth  <-  935 Crown of the Earth
--   98065 Crown of the Earth  <-  98046 Crown of the Earth
--   98386 Alther's Mill  <-  89 The Everstill Bridge
--   98435 Thunderhorn's Report  <-  760 Wildmane Cleansing
--   99049 Threat from Below  <-  99048 A Missing Hand
--   99051 Threat from Below  <-  99049 Threat from Below
--   99052 Threat from Below  <-  99051 Threat from Below
--   99073 Easing Suffering  <-  99050 The Great Tree Provides
--   99129 A Man About a Murloc  <-  99128 Slimy Menace
--   99152 As Above, So Below  <-  96896 A Righteous Cause
--   99153 The One That Got Away  <-  96896 A Righteous Cause
--   99160 Rime's Wrath  <-  99159 Finding Warmth
--   99161 Rime's Wrath  <-  99160 Rime's Wrath
--   99162 Treacherous Cold  <-  99159 Finding Warmth

-- 2) One previous quest, from Wowhead quest series or the quest text (Questie has no chain for these)
INSERT INTO `quest_template_addon` (`ID`,`PrevQuestID`) VALUES
(88758,87318),
(91285,91282),
(91860,91859),
(91861,91860),
(91862,91861),
(93035,93034),
(93128,93062),
(93227,93224),
(94495,94494),
(94872,93225),
(94873,93225),
(94914,94912),
(95111,95034),
(95125,95111),
(95126,95125),
(95140,95126),
(95508,95621),
(96204,91862),
(98219,98191),
(98221,473),
(98300,98297),
(98394,98393),
(98396,98394),
(98404,98341),
(98731,98396)
ON DUPLICATE KEY UPDATE `PrevQuestID` = VALUES(`PrevQuestID`);
--   88758 Sentiments of the Lost  <-  87318 Hearts of the Lost: Wowhead series Hearts of the Lost (Accessories of the Lost between them is not in the game here)
--   91285 Murlocs at the Gates  <-  91282 A Second Home: turn-in NPC of A Second Home gives it; its follow-up Touring the Grounds already needs it
--   91860 A Grim Fate  <-  91859 A Curious Pair: Wowhead series Diplomatic Incident step 3
--   91861 Into Fenris Keep  <-  91860 A Grim Fate: Wowhead series step 4
--   91862 Lumina Windsinger  <-  91861 Into Fenris Keep: Wowhead series step 5
--   93035 Ravaging Ravagers  <-  93034 Valley of Bones: Wowhead series: after Valley of Bones, next to Outing Outcasts which already needs it
--   93128 Khan Jehn  <-  93062 Bone Burying: Wowhead series Bone Burying > Checking Things Out (not in the game here) > Khan Jehn
--   93227 Buying Friendship: Gelkis  <-  93224 Carved Centaur Totems: needs the bundle of carved totems from Carved Centaur Totems
--   94495 Call of Water  <-  94494 Call of Water: Wowhead series Call of Water, Norric takes the first one in
--   94872 Buying Friendship: Magram  <-  93225 Carved Centaur Totems: needs the bundle of carved totems from Carved Centaur Totems
--   94873 Buying Friendship: Gelkis  <-  93225 Carved Centaur Totems: needs the bundle of carved totems from Carved Centaur Totems
--   94914 Moonglade  <-  94912 Child of Nature: the Alliance Child of Nature ends at this giver; the Horde Moonglade already needs its Child of Nature
--   95111 An Underrated Talent  <-  95034 The Debt: Wowhead series The Debt (A Moon-Kissed Blade between them is not in the game here)
--   95125 Ott's Masterwork  <-  95111 An Underrated Talent: Wowhead series
--   95126 The Moonsilver Blade  <-  95125 Ott's Masterwork: Wowhead series
--   95140 Old Fire-Eye  <-  95126 The Moonsilver Blade: text: 'I see that you have your weapon now' after The Moonsilver Blade
--   95508 Unwelcome Guests  <-  95621 Trouble in the Valley: text: the deserter story from Trouble in the Valley
--   96204 The Windshaper's Wrath  <-  91862 Lumina Windsinger: Wowhead series step 6
--   98219 The Shamed Lieutenant  <-  98191 A Friend of the Family: text: 'Thank you again for delivering the statuette'
--   98221 From the Ashes  <-  473 Report to Captain Stoutfist: Wowhead series: next to War Banners, which already needs it
--   98300 Word to Ironforge  <-  98297 Stopping the Cycle: text: last part of Howin's story after Stopping the Cycle
--   98394 The Great Cat Spirit  <-  98393 The Great Cat Spirit: Wowhead series (Alliance cat form, same as the Horde chain)
--   98396 The Great Cat Spirit  <-  98394 The Great Cat Spirit: Wowhead series
--   98404 The Great Windborne Cat Spirit  <-  98341 The Great Windborne Cat Spirit: Wowhead series (Windborne cat form)
--   98731 Blessings of the Great Cat Spirit  <-  98396 The Great Cat Spirit: Wowhead series

-- 3) Several previous quests, as conditions (19 = quest available, 8 = quest rewarded)
UPDATE `quest_template_addon` SET `PrevQuestID` = 0 WHERE `ID` IN (87760,92515,92516,92553,92745,92752,93165,93318,93319,93951,94485,94486,94487,94490,96057,96899,97286,97925,98285);
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 19 AND `ConditionTypeOrReference` = 8 AND `SourceEntry` IN (87760,92515,92516,92553,92745,92752,93165,93318,93319,93951,94485,94486,94487,94490,96057,96899,97286,97925,98285);
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`NegativeCondition`,`Comment`) VALUES
(19,0,87760,0,0,8,0,4722,0,0,0,'Holy Diver: needs Beached Sea Turtle (4722) and the rest of its group'),
(19,0,87760,0,0,8,0,4723,0,0,0,'Holy Diver: needs Beached Sea Creature (4723) and the rest of its group'),
(19,0,87760,0,0,8,0,4725,0,0,0,'Holy Diver: needs Beached Sea Turtle (4725) and the rest of its group'),
(19,0,87760,0,0,8,0,4727,0,0,0,'Holy Diver: needs Beached Sea Turtle (4727) and the rest of its group'),
(19,0,87760,0,0,8,0,4728,0,0,0,'Holy Diver: needs Beached Sea Creature (4728) and the rest of its group'),
(19,0,87760,0,0,8,0,4730,0,0,0,'Holy Diver: needs Beached Sea Creature (4730) and the rest of its group'),
(19,0,87760,0,0,8,0,4731,0,0,0,'Holy Diver: needs Beached Sea Turtle (4731) and the rest of its group'),
(19,0,87760,0,0,8,0,4732,0,0,0,'Holy Diver: needs Beached Sea Turtle (4732) and the rest of its group'),
(19,0,87760,0,0,8,0,4733,0,0,0,'Holy Diver: needs Beached Sea Creature (4733) and the rest of its group'),
(19,0,92745,0,0,8,0,92742,0,0,0,'The State of the Mines: needs Testing the Wells (92742) and the rest of its group'),
(19,0,92745,0,0,8,0,92744,0,0,0,'The State of the Mines: needs Murloc Gills (92744) and the rest of its group'),
(19,0,92752,0,0,8,0,92749,0,0,0,'Explosive Consultation: needs A Dynamite Plan (92749) and the rest of its group'),
(19,0,92752,0,0,8,0,92751,0,0,0,'Explosive Consultation: needs Detonation at a Distance (92751) and the rest of its group'),
(19,0,94490,0,0,8,0,94485,0,0,0,'Ripped Missive: needs Tears of the Lady (94485) and the rest of its group'),
(19,0,94490,0,0,8,0,94486,0,0,0,'Ripped Missive: needs Feathers for Binding (94486) and the rest of its group'),
(19,0,94490,0,0,8,0,94487,0,0,0,'Ripped Missive: needs Unwanted and Unworthy (94487) and the rest of its group'),
(19,0,96899,0,0,8,0,96897,0,0,0,'Bandarion Keep: needs The Cult of the Damned (96897) and the rest of its group'),
(19,0,96899,0,0,8,0,96898,0,0,0,'Bandarion Keep: needs Remnants of War (96898) and the rest of its group'),
(19,0,92515,0,0,8,0,92514,0,0,0,'The Problem With Prideclaws: needs any one of its group, here Welcome to Shen''dar Village (92514)'),
(19,0,92515,0,1,8,0,93461,0,0,0,'The Problem With Prideclaws: needs any one of its group, here Welcome to Shen''dar Village (93461)'),
(19,0,92516,0,0,8,0,92514,0,0,0,'Hippogryph Harassment: needs any one of its group, here Welcome to Shen''dar Village (92514)'),
(19,0,92516,0,1,8,0,93461,0,0,0,'Hippogryph Harassment: needs any one of its group, here Welcome to Shen''dar Village (93461)'),
(19,0,92553,0,0,8,0,92514,0,0,0,'Restocking the Larders: needs any one of its group, here Welcome to Shen''dar Village (92514)'),
(19,0,92553,0,1,8,0,93461,0,0,0,'Restocking the Larders: needs any one of its group, here Welcome to Shen''dar Village (93461)'),
(19,0,93165,0,0,8,0,94484,0,0,0,'Mercy Falls on Deaf Ears: needs any one of its group, here Unnerving Silence (94484)'),
(19,0,93165,0,1,8,0,94493,0,0,0,'Mercy Falls on Deaf Ears: needs any one of its group, here A Sacrifice in Vain (94493)'),
(19,0,93318,0,0,8,0,92514,0,0,0,'WANTED: Vulgara the Insatiable: needs any one of its group, here Welcome to Shen''dar Village (92514)'),
(19,0,93318,0,1,8,0,93461,0,0,0,'WANTED: Vulgara the Insatiable: needs any one of its group, here Welcome to Shen''dar Village (93461)'),
(19,0,93319,0,0,8,0,92514,0,0,0,'Pilfered Windstones: needs any one of its group, here Welcome to Shen''dar Village (92514)'),
(19,0,93319,0,1,8,0,93461,0,0,0,'Pilfered Windstones: needs any one of its group, here Welcome to Shen''dar Village (93461)'),
(19,0,93951,0,0,8,0,92514,0,0,0,'A Little Beauty: needs any one of its group, here Welcome to Shen''dar Village (92514)'),
(19,0,93951,0,1,8,0,93461,0,0,0,'A Little Beauty: needs any one of its group, here Welcome to Shen''dar Village (93461)'),
(19,0,94485,0,0,8,0,94484,0,0,0,'Tears of the Lady: needs any one of its group, here Unnerving Silence (94484)'),
(19,0,94485,0,1,8,0,94493,0,0,0,'Tears of the Lady: needs any one of its group, here A Sacrifice in Vain (94493)'),
(19,0,94486,0,0,8,0,94484,0,0,0,'Feathers for Binding: needs any one of its group, here Unnerving Silence (94484)'),
(19,0,94486,0,1,8,0,94493,0,0,0,'Feathers for Binding: needs any one of its group, here A Sacrifice in Vain (94493)'),
(19,0,94487,0,0,8,0,94484,0,0,0,'Unwanted and Unworthy: needs any one of its group, here Unnerving Silence (94484)'),
(19,0,94487,0,1,8,0,94493,0,0,0,'Unwanted and Unworthy: needs any one of its group, here A Sacrifice in Vain (94493)'),
(19,0,96057,0,0,8,0,95998,0,0,0,'Camping 101: Tailoring: needs any one of its group, here The Great Outdoors (95998)'),
(19,0,96057,0,1,8,0,96101,0,0,0,'Camping 101: Tailoring: needs any one of its group, here The Great Outdoors (96101)'),
(19,0,96057,0,2,8,0,96604,0,0,0,'Camping 101: Tailoring: needs any one of its group, here The Great Outdoors (96604)'),
(19,0,96057,0,3,8,0,96605,0,0,0,'Camping 101: Tailoring: needs any one of its group, here The Great Outdoors (96605)'),
(19,0,96057,0,4,8,0,96606,0,0,0,'Camping 101: Tailoring: needs any one of its group, here The Great Outdoors (96606)'),
(19,0,96057,0,5,8,0,96607,0,0,0,'Camping 101: Tailoring: needs any one of its group, here The Great Outdoors (96607)'),
(19,0,96057,0,6,8,0,96608,0,0,0,'Camping 101: Tailoring: needs any one of its group, here The Great Outdoors (96608)'),
(19,0,97286,0,0,8,0,78124,0,0,0,'Research Access: needs any one of its group, here Nar''thalas Almanac (78124)'),
(19,0,97286,0,1,8,0,78127,0,0,0,'Research Access: needs any one of its group, here The Dalaran Digest (78127)'),
(19,0,97286,0,2,8,0,78142,0,0,0,'Research Access: needs any one of its group, here Bewitchments and Glamours (78142)'),
(19,0,97286,0,3,8,0,78143,0,0,0,'Research Access: needs any one of its group, here ? (78143)'),
(19,0,97286,0,4,8,0,78145,0,0,0,'Research Access: needs any one of its group, here Arcanic Systems Manual (78145)'),
(19,0,97286,0,5,8,0,78146,0,0,0,'Research Access: needs any one of its group, here Goaz Scrolls (78146)'),
(19,0,97286,0,6,8,0,78147,0,0,0,'Research Access: needs any one of its group, here Crimes Against Anatomy (78147)'),
(19,0,97286,0,7,8,0,78148,0,0,0,'Research Access: needs any one of its group, here Runes of the Sorcerer-Kings (78148)'),
(19,0,97286,0,8,8,0,78149,0,0,0,'Research Access: needs any one of its group, here Fury of the Land (78149)'),
(19,0,97286,0,9,8,0,79091,0,0,0,'Research Access: needs any one of its group, here Archmage Antonidas: The Unabridged Autobiography (79091)'),
(19,0,97286,0,10,8,0,79092,0,0,0,'Research Access: needs any one of its group, here Archmage Theocritus'' Research Journal (79092)'),
(19,0,97286,0,11,8,0,79093,0,0,0,'Research Access: needs any one of its group, here Rumi of Gnomeregan: The Collected Works (79093)'),
(19,0,97286,0,12,8,0,79094,0,0,0,'Research Access: needs any one of its group, here The Lessons of Ta''zo (79094)'),
(19,0,97286,0,13,8,0,79095,0,0,0,'Research Access: needs any one of its group, here The Apothecary''s Metaphysical Primer (79095)'),
(19,0,97286,0,14,8,0,79096,0,0,0,'Research Access: needs any one of its group, here ? (79096)'),
(19,0,97286,0,15,8,0,79097,0,0,0,'Research Access: needs any one of its group, here Baxtan: On Destructive Magics (79097)'),
(19,0,97286,0,16,8,0,79535,0,0,0,'Research Access: needs any one of its group, here Basilisks: Should Petrification be Feared? (79535)'),
(19,0,97286,0,17,8,0,79947,0,0,0,'Research Access: needs any one of its group, here Geomancy: The Stone-Cold Truth (79947)'),
(19,0,97286,0,18,8,0,79948,0,0,0,'Research Access: needs any one of its group, here Defensive Magics 101 (79948)'),
(19,0,97286,0,19,8,0,79949,0,0,0,'Research Access: needs any one of its group, here A Web of Lies: Debunking Myths and Legends (79949)'),
(19,0,97286,0,20,8,0,79950,0,0,0,'Research Access: needs any one of its group, here Demons and You (79950)'),
(19,0,97286,0,21,8,0,79951,0,0,0,'Research Access: needs any one of its group, here Mummies: A Guide to the Unsavory Undead (79951)'),
(19,0,97286,0,22,8,0,79952,0,0,0,'Research Access: needs any one of its group, here RwlRwlRwlRwl! (79952)'),
(19,0,97286,0,23,8,0,79953,0,0,0,'Research Access: needs any one of its group, here ? (79953)'),
(19,0,97286,0,24,8,0,81947,0,0,0,'Research Access: needs any one of its group, here Sanguine Sorcery (81947)'),
(19,0,97286,0,25,8,0,81949,0,0,0,'Research Access: needs any one of its group, here Legends of the Tidesages (81949)'),
(19,0,97286,0,26,8,0,81951,0,0,0,'Research Access: needs any one of its group, here ? (81951)'),
(19,0,97286,0,27,8,0,81952,0,0,0,'Research Access: needs any one of its group, here Everyday Etiquette (81952)'),
(19,0,97286,0,28,8,0,81953,0,0,0,'Research Access: needs any one of its group, here Stonewrought Design (81953)'),
(19,0,97286,0,29,8,0,81954,0,0,0,'Research Access: needs any one of its group, here Venomous Journeys (81954)'),
(19,0,97286,0,30,8,0,81955,0,0,0,'Research Access: needs any one of its group, here A Mind of Metal (81955)'),
(19,0,97286,0,31,8,0,81956,0,0,0,'Research Access: needs any one of its group, here Conjurer''s Codex (81956)'),
(19,0,97286,0,32,8,0,84395,0,0,0,'Research Access: needs any one of its group, here Undead Potatoes (84395)'),
(19,0,97286,0,33,8,0,84396,0,0,0,'Research Access: needs any one of its group, here Magma or Lava? (84396)'),
(19,0,97286,0,34,8,0,84397,0,0,0,'Research Access: needs any one of its group, here Northern Kalimdor - A Comprehensive Guide (84397)'),
(19,0,97286,0,35,8,0,84398,0,0,0,'Research Access: needs any one of its group, here A Study of the Light (84398)'),
(19,0,97286,0,36,8,0,84399,0,0,0,'Research Access: needs any one of its group, here Ka-Boom! (84399)'),
(19,0,97286,0,37,8,0,84400,0,0,0,'Research Access: needs any one of its group, here The Knight and the Lady (84400)'),
(19,0,97286,0,38,8,0,84401,0,0,0,'Research Access: needs any one of its group, here Scourge: Undead Menace or Misunderstood? (84401)'),
(19,0,97286,0,39,8,0,84402,0,0,0,'Research Access: needs any one of its group, here Necromancy 101 (84402)'),
(19,0,97925,0,0,8,0,95998,0,0,0,'Camping 101: Tailoring: needs any one of its group, here The Great Outdoors (95998)'),
(19,0,97925,0,1,8,0,96101,0,0,0,'Camping 101: Tailoring: needs any one of its group, here The Great Outdoors (96101)'),
(19,0,97925,0,2,8,0,96604,0,0,0,'Camping 101: Tailoring: needs any one of its group, here The Great Outdoors (96604)'),
(19,0,97925,0,3,8,0,96605,0,0,0,'Camping 101: Tailoring: needs any one of its group, here The Great Outdoors (96605)'),
(19,0,97925,0,4,8,0,96606,0,0,0,'Camping 101: Tailoring: needs any one of its group, here The Great Outdoors (96606)'),
(19,0,97925,0,5,8,0,96607,0,0,0,'Camping 101: Tailoring: needs any one of its group, here The Great Outdoors (96607)'),
(19,0,97925,0,6,8,0,96608,0,0,0,'Camping 101: Tailoring: needs any one of its group, here The Great Outdoors (96608)'),
(19,0,98285,0,0,8,0,95998,0,0,0,'Camping 101: Engineering: needs any one of its group, here The Great Outdoors (95998)'),
(19,0,98285,0,1,8,0,96101,0,0,0,'Camping 101: Engineering: needs any one of its group, here The Great Outdoors (96101)'),
(19,0,98285,0,2,8,0,96604,0,0,0,'Camping 101: Engineering: needs any one of its group, here The Great Outdoors (96604)'),
(19,0,98285,0,3,8,0,96605,0,0,0,'Camping 101: Engineering: needs any one of its group, here The Great Outdoors (96605)'),
(19,0,98285,0,4,8,0,96606,0,0,0,'Camping 101: Engineering: needs any one of its group, here The Great Outdoors (96606)'),
(19,0,98285,0,5,8,0,96607,0,0,0,'Camping 101: Engineering: needs any one of its group, here The Great Outdoors (96607)'),
(19,0,98285,0,6,8,0,96608,0,0,0,'Camping 101: Engineering: needs any one of its group, here The Great Outdoors (96608)');

-- 4) Unnerving Silence and A Sacrifice in Vain are two ways into the same Nightclaw Den story: only one of them (Questie: exclusiveTo).
INSERT INTO `quest_template_addon` (`ID`,`ExclusiveGroup`) VALUES (94484,94484),(94493,94484)
ON DUPLICATE KEY UPDATE `ExclusiveGroup` = VALUES(`ExclusiveGroup`);
