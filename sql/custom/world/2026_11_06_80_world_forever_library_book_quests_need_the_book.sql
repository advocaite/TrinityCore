-- Library book quests: the librarians only offer a book quest to a player who carries that book.
-- Player report 2026-11-06: since patch 0.1.30 one NPC offers dozens of book quests in red (levels 20, 35, 45 and 60) even to
-- low level characters, and they read as if quest limits were gone. Level, race and class checks did not change. The world
-- file 2026_11_05_99 (restore of Forever quest data) gave Garion Wendell (211033) 28 book quests, Jennea Cannon (5497) 8 and
-- Owen Thadd (211022) 2, and all of them have MinLevel 1 in quest_classic_level, so the level check lets everyone see them.
-- The book objects stay in the world and the quests still work: pick up a book and its quest appears at the librarian.
-- Safe to run again.

-- Books that drop from a spawned object: offered only with the book in the bags.
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 19 AND `ConditionTypeOrReference` = 2 AND `SourceEntry` IN
(78124,78127,78142,78145,78147,78149,79091,79092,79093,79095,79097,79947,79948,79949,79950,79951,79952,81949,81954,81955,81956,84395,84396,84397,84398,84399,84400,84401,84402);
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`NegativeCondition`,`Comment`) VALUES
(19,0,78124,0,0,2,0,209843,1,0,0,'Nar''thalas Almanac: only with the book'),
(19,0,78127,0,0,2,0,209844,1,0,0,'The Dalaran Digest: only with the book'),
(19,0,78142,0,0,2,0,209845,1,0,0,'Bewitchments and Glamours: only with the book'),
(19,0,78145,0,0,2,0,209847,1,0,0,'Arcanic Systems Manual: only with the book'),
(19,0,78147,0,0,2,0,209849,1,0,0,'Crimes Against Anatomy: only with the book'),
(19,0,78149,0,0,2,0,209851,1,0,0,'Fury of the Land: only with the book'),
(19,0,79091,0,0,2,0,203754,1,0,0,'Archmage Antonidas: The Unabridged Autobiography: only with the book'),
(19,0,79092,0,0,2,0,203755,1,0,0,'Archmage Theocritus'' Research Journal: only with the book'),
(19,0,79093,0,0,2,0,208860,1,0,0,'Rumi of Gnomeregan: The Collected Works: only with the book'),
(19,0,79095,0,0,2,0,208185,1,0,0,'The Apothecary''s Metaphysical Primer: only with the book'),
(19,0,79097,0,0,2,0,208800,1,0,0,'Baxtan: On Destructive Magics: only with the book'),
(19,0,79947,0,0,2,0,215683,1,0,0,'Geomancy: The Stone-Cold Truth: only with the book'),
(19,0,79948,0,0,2,0,215815,1,0,0,'Defensive Magics 101: only with the book'),
(19,0,79949,0,0,2,0,215816,1,0,0,'A Web of Lies: only with the book'),
(19,0,79950,0,0,2,0,215817,1,0,0,'Demons and You: only with the book'),
(19,0,79951,0,0,2,0,215820,1,0,0,'Mummies: only with the book'),
(19,0,79952,0,0,2,0,215822,1,0,0,'RwlRwlRwlRwl!: only with the book'),
(19,0,81949,0,0,2,0,220346,1,0,0,'Legends of the Tidesages: only with the book'),
(19,0,81954,0,0,2,0,220350,1,0,0,'Venomous Journeys: only with the book'),
(19,0,81955,0,0,2,0,220352,1,0,0,'A Mind of Metal: only with the book'),
(19,0,81956,0,0,2,0,220353,1,0,0,'Conjurer''s Codex: only with the book'),
(19,0,84395,0,0,2,0,228132,1,0,0,'Undead Potatoes: only with the book'),
(19,0,84396,0,0,2,0,228133,1,0,0,'Magma or Lava?: only with the book'),
(19,0,84397,0,0,2,0,228134,1,0,0,'Northern Kalimdor: only with the book'),
(19,0,84398,0,0,2,0,228135,1,0,0,'A Study of the Light: only with the book'),
(19,0,84399,0,0,2,0,228136,1,0,0,'Ka-Boom!: only with the book'),
(19,0,84400,0,0,2,0,228138,1,0,0,'The Knight and the Lady: only with the book'),
(19,0,84401,0,0,2,0,228140,1,0,0,'Scourge: Undead Menace or Misunderstood?: only with the book'),
(19,0,84402,0,0,2,0,228141,1,0,0,'Necromancy 101: only with the book');

-- Book quests with no objective in our data: anyone could take and hand them in for the reward without a book, and no loot
-- gives their book. Not offered until they get their real required item, like the Sealed Crate quests. Hand-in rows stay, so a
-- player who already took one can finish it.
DELETE FROM `creature_queststarter` WHERE `id` = 211033 AND `quest` IN (78146,78148,78150,79535,79536,81947,81952,81953);
DELETE FROM `creature_queststarter` WHERE `id` = 211022 AND `quest` = 79094;

-- Research Access (mage, quest level 20, no objective) waits until level 20 instead of showing red from level 1.
UPDATE `quest_classic_level` SET `MinLevel` = 20 WHERE `ID` = 97286;
