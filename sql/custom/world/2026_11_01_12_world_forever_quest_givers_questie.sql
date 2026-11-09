-- Quest givers and enders seen on Forever but missing in our world: 35 start and 30 end links on NPCs,
-- 3 start and 1 end link on objects. Most of these quests had no giver at all, so they could never be taken.
-- Source: Questie Forever traces (Questie team, Questie/QuestieDB, GPLv3), traces/foreverQuestTraces.lua
-- startedBy/finishedBy, from player sessions on the Forever beta. Only NPCs/objects that already exist and are
-- spawned in our world are used; the rest (no template or no spawn) is listed in the report, not here.
-- Additive only: existing givers stay.
INSERT IGNORE INTO `creature_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES
(270459,6121,0),
(5905,79361,0),
(257655,94438,0),
(257663,94440,0),
(258098,94449,0),
(258113,94449,0),
(257597,94465,0),
(258098,94494,0),
(377,94774,0),
(1265,95212,0),
(3682,95494,0),
(3682,95495,0),
(3682,95507,0),
(1246,96045,0),
(11065,96059,0),
(264072,96260,0),
(259118,96261,0),
(2981,96659,0),
(3194,96821,0),
(266940,96822,0),
(1738,96895,0),
(267009,96899,0),
(7953,97223,0),
(3304,97225,0),
(254082,97243,0),
(2094,98240,0),
(11802,98341,0),
(272054,98404,0),
(272054,98738,0),
(9087,98823,0),
(3142,99052,0),
(1253,99159,0),
(1253,99160,0),
(1253,99161,0),
(1253,99162,0);
INSERT IGNORE INTO `creature_questender` (`id`,`quest`,`VerifiedBuild`) VALUES
(211022,79097,0),
(211033,79097,0),
(5905,79361,0),
(253204,92850,0),
(257663,94438,0),
(257597,94449,0),
(257597,94468,0),
(1265,95212,0),
(3682,95494,0),
(3682,95507,0),
(9087,95664,0),
(956,95795,0),
(1241,96044,0),
(2326,96047,0),
(264072,96260,0),
(259118,96261,0),
(3139,96821,0),
(266940,96822,0),
(7953,97223,0),
(3188,97225,0),
(11068,97917,0),
(3649,98209,0),
(272054,98341,0),
(272054,98404,0),
(11802,98738,0),
(3142,99052,0),
(1253,99159,0),
(1253,99160,0),
(1253,99161,0),
(1253,99162,0);
INSERT IGNORE INTO `gameobject_queststarter` (`id`,`quest`,`VerifiedBuild`) VALUES
(61934,94468,0),
(556669,567,0),
(556670,549,0);
INSERT IGNORE INTO `gameobject_questender` (`id`,`quest`,`VerifiedBuild`) VALUES
(61934,94467,0);
-- The NPCs need the quest giver flag (2) to show the quest.
UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry` IN (377,956,1241,1246,1253,1265,1738,2094,2326,2981,3139,3142,3188,3194,3304,3649,3682,5905,7953,9087,11065,11068,11802,211022,211033,253204,254082,257597,257655,257663,258098,258113,259118,264072,266940,267009,270459,272054) AND (`npcflag`&2)=0;
-- List (npc -> quest):
--   start 270459 Alfina Nightgaze -> 6121 
--   start 5905 Prate Cloudseer -> 79361 Storm, Earth, and Fire
--   start 257655 Deathguard Billmuth -> 94438 A Lesson in Divinity
--   start 257663 Deathguard Falgan -> 94440 A Lesson in Divinity
--   start 258098 Eldrun Stormbreaker -> 94449 Call of Fire
--   start 258113 Ingrid Dunwald -> 94449 Call of Fire
--   start 257597 Bruegs Kindleborn -> 94465 Call of Fire
--   start 258098 Eldrun Stormbreaker -> 94494 Call of Water
--   start 377 Priestess Josetta -> 94774 Divine Grace
--   start 1265 Rudra Amberstill -> 95212 Never Saddle on Quality
--   start 3682 Vrang Wildgore -> 95494 Bruised Pride and Lion Hides
--   start 3682 Vrang Wildgore -> 95495 The Hermit Tanner
--   start 3682 Vrang Wildgore -> 95507 Vrang's Game
--   start 1246 Vosur Brakthel -> 96045 Camping 101: Alchemy
--   start 11065 Thonys Pillarstone -> 96059 Camping 101: Enchanting
--   start 264072 Yorn Grimtotem -> 96260 Stronger than Steel
--   start 259118 Muln Earthfury -> 96261 The Broodmother
--   start 2981 Chief Hawkwind -> 96659 The Adventurer
--   start 3194 Vel'rin Fang -> 96821 Legging It
--   start 266940 Turroc -> 96822 For Honor
--   start 1738 Deathguard Terrence -> 96895 The Argent Emissary
--   start 267009 Hadric Harlson -> 96899 Bandarion Keep
--   start 7953 Xar'Ti -> 97223 Bloodtalon Matriarch
--   start 3304 Master Vornal -> 97225 Forgotten Loa Idols
--   start 254082 Aarnor Galestrike -> 97243 Call of Fire
--   start 2094 James Halloran -> 98240 Crimson Crate Delivery
--   start 11802 Dendrite Starblaze -> 98341 The Great Windborne Cat Spirit
--   start 272054 Avatar of Saeyleenan -> 98404 The Great Windborne Cat Spirit
--   start 272054 Avatar of Saeyleenan -> 98738 Blessings of the Great Windborne Cat Spirit
--   start 9087 Bashana Runetotem -> 98823 Earthen Echo
--   start 3142 Orgnil Soulscar -> 99052 Threat from Below
--   start 1253 Father Gavin -> 99159 Finding Warmth
--   start 1253 Father Gavin -> 99160 Rime's Wrath
--   start 1253 Father Gavin -> 99161 Rime's Wrath
--   start 1253 Father Gavin -> 99162 Treacherous Cold
--   end   211022 Owen Thadd -> 79097 Baxtan: On Destructive Magics
--   end   211033 Garion Wendell -> 79097 Baxtan: On Destructive Magics
--   end   5905 Prate Cloudseer -> 79361 Storm, Earth, and Fire
--   end   253204 Dondallion Whisperwind -> 92850 The Missing Scholar
--   end   257663 Deathguard Falgan -> 94438 A Lesson in Divinity
--   end   257597 Bruegs Kindleborn -> 94449 Call of Fire
--   end   257597 Bruegs Kindleborn -> 94468 Call of Fire
--   end   1265 Rudra Amberstill -> 95212 Never Saddle on Quality
--   end   3682 Vrang Wildgore -> 95494 Bruised Pride and Lion Hides
--   end   3682 Vrang Wildgore -> 95507 Vrang's Game
--   end   9087 Bashana Runetotem -> 95664 Elder Knowledge
--   end   956 Dorin Songblade -> 95795 Fallen in the Fen
--   end   1241 Tognus Flintfire -> 96044 Camping 101: Blacksmithing
--   end   2326 Thamner Pol -> 96047 Camping 101: First Aid
--   end   264072 Yorn Grimtotem -> 96260 Stronger than Steel
--   end   259118 Muln Earthfury -> 96261 The Broodmother
--   end   3139 Gar'Thok -> 96821 Legging It
--   end   266940 Turroc -> 96822 For Honor
--   end   7953 Xar'Ti -> 97223 Bloodtalon Matriarch
--   end   3188 Master Gadrin -> 97225 Forgotten Loa Idols
--   end   11068 Betty Quin -> 97917 Camping 101: Enchanting
--   end   3649 Thundris Windweaver -> 98209 Bloom of the Heavens
--   end   272054 Avatar of Saeyleenan -> 98341 The Great Windborne Cat Spirit
--   end   272054 Avatar of Saeyleenan -> 98404 The Great Windborne Cat Spirit
--   end   11802 Dendrite Starblaze -> 98738 Blessings of the Great Windborne Cat Spirit
--   end   3142 Orgnil Soulscar -> 99052 Threat from Below
--   end   1253 Father Gavin -> 99159 Finding Warmth
--   end   1253 Father Gavin -> 99160 Rime's Wrath
--   end   1253 Father Gavin -> 99161 Rime's Wrath
--   end   1253 Father Gavin -> 99162 Treacherous Cold
--   object 61934 Brazier of the Dormant Flame -> 94468 Call of Fire
--   object 556669 Dangerous! -> 567 
--   object 556670 WANTED -> 549 
--   object 61934 Brazier of the Dormant Flame -> 94467 Call of Fire
