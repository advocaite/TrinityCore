-- Hunter pets (0.1.36): which beasts can be tamed, as Blizzard's live realm says (client 1.60.1.70888, 11-11).
-- CreatureTemplate::IsTameable needs a beast (type 1), a pet family and TypeFlags bit 1 (CREATURE_TYPE_FLAG_TAMEABLE).
-- Sources: a player's enUS cache folder written 2026-11-11 15:03 (cache-chris-3-1111.zip, creaturecache.wdb) marked "cache",
-- and the 2026-11-11 21:08 ruRU live capture as compared in cache-sniff-1111/compare-live-vs-ours-1111.json and
-- creatures-report-only-1111.json, marked "sniff". The cache pass file 2026_11_11_50 was meant to hold sections 1 and 2 of this
-- but came out with only its model rows, so the tameable rows live here. If _50 is rebuilt with them, both say the same.
-- Every change sets the live value, so running it twice or after a fixed _50 changes nothing.

-- 1. Beasts live marks tameable that ours did not (Discord 11-11, Andy: beasts on the new island can't be tamed). The 13 from
--    the cache spawn around the new eastern island and coast of map 0 (Paletusks, crawlers, buzzards, Tallstrider...).
UPDATE `creature_template` SET `family` = 5 WHERE `entry` = 255851; -- Mature Paletusk (cache)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` | 1 WHERE `Entry` = 255851; -- Mature Paletusk (cache)
UPDATE `creature_template` SET `family` = 5 WHERE `entry` = 258331; -- Young Paletusk (cache)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` | 1 WHERE `Entry` = 258331; -- Young Paletusk (cache)
UPDATE `creature_template` SET `family` = 1 WHERE `entry` = 259267; -- Plains Coyote (cache)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` | 1 WHERE `Entry` = 259267; -- Plains Coyote (cache)
UPDATE `creature_template` SET `family` = 7 WHERE `entry` = 259269; -- High Plains Buzzard (cache)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` | 1 WHERE `Entry` = 259269; -- High Plains Buzzard (cache)
UPDATE `creature_template` SET `family` = 7 WHERE `entry` = 263343; -- Low Plains Buzzard (cache)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` | 1 WHERE `Entry` = 263343; -- Low Plains Buzzard (cache)
UPDATE `creature_template` SET `family` = 12 WHERE `entry` = 263348; -- Roving Tallstrider (cache)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` | 1 WHERE `Entry` = 263348; -- Roving Tallstrider (cache)
UPDATE `creature_template` SET `family` = 1 WHERE `entry` = 268558; -- Chakuyak (cache)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` | 1 WHERE `Entry` = 268558; -- Chakuyak (cache)
UPDATE `creature_template` SET `family` = 6 WHERE `entry` = 259046; -- Freshwater Crocolisk (cache)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` | 1 WHERE `Entry` = 259046; -- Freshwater Crocolisk (cache)
UPDATE `creature_template` SET `family` = 8 WHERE `entry` = 260235; -- Trapclaw (cache)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` | 1 WHERE `Entry` = 260235; -- Trapclaw (cache)
UPDATE `creature_template` SET `family` = 8 WHERE `entry` = 256807; -- Shore Crawler (cache)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` | 1 WHERE `Entry` = 256807; -- Shore Crawler (cache)
UPDATE `creature_template` SET `family` = 8 WHERE `entry` = 255901; -- Sand Crawler (cache)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` | 1 WHERE `Entry` = 255901; -- Sand Crawler (cache)
UPDATE `creature_template` SET `family` = 24 WHERE `entry` = 262037; -- Roosting Duskbat (cache)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` | 1 WHERE `Entry` = 262037; -- Roosting Duskbat (cache)
UPDATE `creature_template` SET `family` = 4 WHERE `entry` = 248299; -- Elmpaw (cache)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` | 1 WHERE `Entry` = 248299; -- Elmpaw (cache)

-- 2. Forever rares live marks tameable, with the family live sends (sniff; ours had family 0; the tameable flag as read by the
--    cache pass, README "20 beasts live marks tameable").
UPDATE `creature_template` SET `family` = 2 WHERE `entry` = 276111; -- Ghostfang (sniff)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` | 1 WHERE `Entry` = 276111; -- Ghostfang (sniff)
UPDATE `creature_template` SET `family` = 26 WHERE `entry` = 276101; -- Nightscreech (sniff)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` | 1 WHERE `Entry` = 276101; -- Nightscreech (sniff)
UPDATE `creature_template` SET `family` = 4 WHERE `entry` = 276189; -- Matriarch Bristlefur (sniff)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` | 1 WHERE `Entry` = 276189; -- Matriarch Bristlefur (sniff)
UPDATE `creature_template` SET `family` = 2 WHERE `entry` = 276198; -- Shal'ma (sniff)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` | 1 WHERE `Entry` = 276198; -- Shal'ma (sniff)
UPDATE `creature_template` SET `family` = 12 WHERE `entry` = 256492; -- Fernfeather (sniff)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` | 1 WHERE `Entry` = 256492; -- Fernfeather (sniff)
UPDATE `creature_template` SET `family` = 319 WHERE `entry` = 255887; -- Slydris (sniff)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` | 1 WHERE `Entry` = 255887; -- Slydris (sniff)
UPDATE `creature_template` SET `family` = 3 WHERE `entry` = 272096; -- Befouled Webwood (sniff)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` | 1 WHERE `Entry` = 272096; -- Befouled Webwood (sniff)

-- 3. Ravasaur Matriarch: live family 11 (raptor), ours 0 (sniff). Only the family; the tameable flag was not in the report.
UPDATE `creature_template` SET `family` = 11 WHERE `entry` = 6581; -- Ravasaur Matriarch (sniff)

-- 4. Beasts live does NOT let you tame any more (21 vanilla rares and quest beasts). We follow live: Forever is Blizzard's game
--    and these answers are its current server data. 2026_11_09_12 (open world rule, INFERRED) had made some of them tameable.
--    A pet a player already tamed from one of these stays (nothing here touches character pets).
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` & ~1 WHERE `Entry` = 3056; -- Ghost Howl (cache)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` & ~1 WHERE `Entry` = 3652; -- Trigore the Lasher (cache)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` & ~1 WHERE `Entry` = 3722; -- Mystlash Flayer (cache)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` & ~1 WHERE `Entry` = 4016; -- Fey Dragon (cache)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` & ~1 WHERE `Entry` = 4018; -- Antlered Courser (cache)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` & ~1 WHERE `Entry` = 4019; -- Great Courser (cache)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` & ~1 WHERE `Entry` = 5347; -- Antilus the Soarer (cache)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` & ~1 WHERE `Entry` = 5831; -- Swiftmane (sniff)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` & ~1 WHERE `Entry` = 5928; -- Sorrow Wing (cache)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` & ~1 WHERE `Entry` = 6582; -- Clutchmother Zavas (cache)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` & ~1 WHERE `Entry` = 6584; -- King Mosh (cache)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` & ~1 WHERE `Entry` = 8302; -- Deatheye (sniff)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` & ~1 WHERE `Entry` = 9042; -- Verek (sniff)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` & ~1 WHERE `Entry` = 11789; -- Deep Borer (cache)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` & ~1 WHERE `Entry` = 12207; -- Thessala Hydra (cache)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` & ~1 WHERE `Entry` = 13323; -- Subterranean Diemetradon (cache)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` & ~1 WHERE `Entry` = 13596; -- Rotgrip (cache)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` & ~1 WHERE `Entry` = 14237; -- Oozeworm (cache)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` & ~1 WHERE `Entry` = 14473; -- Lapress (sniff)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` & ~1 WHERE `Entry` = 14474; -- Zora (sniff)
UPDATE `creature_template_difficulty` SET `TypeFlags` = `TypeFlags` & ~1 WHERE `Entry` = 14475; -- Rex Ashil (sniff)

-- 5. The Razza (Dire Maul): live sends no pet family (ours 38). Follow live.
UPDATE `creature_template` SET `family` = 0 WHERE `entry` = 11497; -- The Razza, ours 38 (sniff)
