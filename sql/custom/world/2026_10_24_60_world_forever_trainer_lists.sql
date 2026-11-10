-- Classic 1.60 (WoW Forever): seven Forever trainers with the trainer npcflag and no trainer list ("Train me." never appears,
-- Creature::GetTrainerId() is 0). They come from the 70124 sniff import (2026_10_01_04_world_forever_sniff_npc.sql) without a
-- creature_trainer row because nobody opened their list in a sniff. Each gets the list of a sniffed Forever trainer of the same
-- profession and rank, or the vMaNGOS list where Forever has none of that rank:
--
--   Dalaran (map 0), the four profession trainers (npcflag 80: trainer + profession trainer, no gossip menu):
--   - Charles Worth 251972, Expert Tailor -> 388, the Forever tailor list (Taleen Shimmerthread 251991, Othesia Evengale 252380).
--     It is the vMaNGOS Expert Tailor list (Sellandus 5567, Rhiannon Davis 11049: same 40 recipes, apprentice and journeyman rank
--     up to skill 150) with Forever's learn spells for the ranks (3911/3912 teach 3908/3909) and Forever's own recipes.
--   - Angelique Butler 251977, First Aid Trainer -> 39, the Forever first aid list (Naleeia Tattermend 257018, and the vanilla
--     first aid trainer Pand Stonebinder 2798 that the sniff moved to 39).
--   - Vanessa Sellers 251978, Enchanting Trainer -> 1001317, the vMaNGOS Expert Enchanter list of Lucan Cordell 1317 (to 150,
--     with the journeyman rank). The only sniffed Forever enchanting list, 357, is the journeyman one (to 70, Mot Dawnstrider 11071),
--     which would leave the Dalaran trainer below the Expert Tailor beside her.
--   - Katherine Lee 252084, Cooking Trainer -> 36, the Forever cooking list (Zerril Softbreeze 251905, Nyalah Brightfire 257006).
--   Druid trainer, Dalaran:
--   - Alfina Nightgaze 270459, Druid Trainer (npcflag 51, as Hana Lighthoof 262560) -> 17, the Forever druid list every other
--     Forever druid trainer uses (Naeluna Swiftmend 254081, Lotheluum Starbreeze 252359, Hana Lighthoof 262560).
--   Durotar:
--   - Pa'zula 266881, Journeyman Enchanter -> 357, the Forever journeyman enchanting list (the sniff moved the vanilla journeyman
--     Mot Dawnstrider 11071 to it; it is the vMaNGOS journeyman list with 7414 for 7411 plus Forever's 1230643).
--   Blacksmith with a "I would like to train." option in his sniffed menu 45017 (npcflag 4305):
--   - Angus Hammerhand 276067 -> 376, the larger Forever blacksmith list (Daeann Steelwind 252479; the smaller 293 of Aedi
--     Thriceforged 251913 is a subset of it).
--
-- Not changed: Boarton Shadetotem 205729 ("Rogue Trainer?", npcflag 19: no class or profession trainer flag) has no data to say
-- what he would teach.
--
-- MenuID 0: none of them has a sniffed menu with a trainer option other than Angus (45017, option 0). The Classic "Train me."
-- option (Player::PrepareGossipMenu) is added for any trainer whose GetTrainerId() is set, and selecting it falls back to that
-- trainer. Safe to run again.

DELETE FROM `creature_trainer` WHERE `CreatureID` IN (251972,251977,251978,252084,270459,266881,276067);
INSERT INTO `creature_trainer` (`CreatureID`,`TrainerID`,`MenuID`,`OptionID`) VALUES
(251972,388,0,0),         -- Charles Worth, Expert Tailor (Dalaran)
(251977,39,0,0),          -- Angelique Butler, First Aid Trainer (Dalaran)
(251978,1001317,0,0),     -- Vanessa Sellers, Enchanting Trainer (Dalaran)
(252084,36,0,0),          -- Katherine Lee, Cooking Trainer (Dalaran)
(270459,17,0,0),          -- Alfina Nightgaze, Druid Trainer (Dalaran)
(266881,357,0,0),         -- Pa'zula, Journeyman Enchanter (Durotar)
(276067,376,45017,0);     -- Angus Hammerhand, Blacksmith
