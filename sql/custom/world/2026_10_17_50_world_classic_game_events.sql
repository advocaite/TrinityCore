-- Classic 1.60: the vanilla world events (VMaNGOS game_event, development database snapshot 2026-09-28).
-- The 95 retail rows referenced Midnight holidays and had no spawns, quests or vendors behind them (game_event_creature and
-- game_event_gameobject were empty), so the whole set is replaced. Dates and lengths are VMaNGOS's (yearly from the 2020
-- calendar); holidayStage stays 0 so the server keeps those dates instead of recomputing them from the client's holiday data.
-- world_event = VMaNGOS hardcoded (started by scripts or world states, never by the clock). VMaNGOS event 152 'Southshore Assassins'
-- is 127 here: the spawn tables' eventEntry is a signed tinyint, so an event above 127 cannot own a spawn. Left out:
--   22 'AQ War Effort (NPC & GO \'Initial\')': patch_max 7, gone by 1.12
--   17 'Scourge Invasion': disabled in VMaNGOS
--   53 'AQ War Effort (NPC Reput Named \'Officer\')': patch_max 7, gone by 1.12
--   52 'AQ War Effort (NPC Reput \'Only War Effort\')': patch_max 7, gone by 1.12
--   54 'AQ War Troop Silithus (NPC & GO) DAY 1': patch_max 7, gone by 1.12
--   55 'AQ War Troop Silithus (NPC & GO) DAY 2': patch_max 7, gone by 1.12
--   56 'AQ War Troop Silithus (NPC & GO) DAY 3': patch_max 7, gone by 1.12
--   57 'AQ War Troop Silithus (NPC & GO) DAY 4': patch_max 7, gone by 1.12
--   58 'AQ War Troop Silithus (NPC & GO) DAY 5': patch_max 7, gone by 1.12
--   59 'AQ War Effort (Cenarion Hold attack)': patch_max 7, gone by 1.12
--   60 'AQ War Troop Silithus 3 (NPC & GO)': patch_max 7, gone by 1.12
--   61 'AQ War Final Battle (NPC)': patch_max 7, gone by 1.12
--   62 'AQ War - Secrets of the Colossus Ashi': patch_max 7, gone by 1.12
--   63 'AQ War - Secrets of the Colossus Regal': patch_max 7, gone by 1.12
--   64 'AQ War - Secrets of the Colossus Zora': patch_max 7, gone by 1.12
--   65 'AQ War - Spawn of Crystals': patch_max 7, gone by 1.12
--   90 'Scourge Invasion - Attacking Winterspring': disabled in VMaNGOS
--   83 'AQ Gate': patch_max 7, gone by 1.12
--   110 'Love is in the Air - Popularity Contest Winner: Darnassus': disabled in VMaNGOS
--   91 'Scourge Invasion - Attacking Tanaris': disabled in VMaNGOS
--   92 'Scourge Invasion - Attacking Azshara': disabled in VMaNGOS
--   93 'Scourge Invasion - Attacking Blasted Lands': disabled in VMaNGOS
--   94 'Scourge Invasion - Attacking Eastern Plaguelands': disabled in VMaNGOS
--   95 'Scourge Invasion - Attacking Burning Steppes': disabled in VMaNGOS
--   96 'Scourge Invasion - 50 Invasions Done': disabled in VMaNGOS
--   97 'Scourge Invasion - 100 Invasions Done': disabled in VMaNGOS
--   98 'Scourge Invasion - 150 Invasions Done': disabled in VMaNGOS
--   99 'Scourge Invasion - Invasions Done': disabled in VMaNGOS
--   111 'Love is in the Air - Popularity Contest Winner: Ironforge': disabled in VMaNGOS
--   112 'Love is in the Air - Popularity Contest Winner: Stormwind': disabled in VMaNGOS
--   113 'Love is in the Air - Popularity Contest Winner: Orgrimmar': disabled in VMaNGOS
--   114 'Love is in the Air - Popularity Contest Winner: Thunder Bluff': disabled in VMaNGOS
--   115 'Love is in the Air - Popularity Contest Winner: Undercity': disabled in VMaNGOS
DELETE FROM `game_event`;
DELETE FROM `game_event_arena_seasons`;
-- (game_event_battleground_holiday is not a table of this world database: the battleground holidays live in the client's holiday data.)
DELETE FROM `game_event_condition`;
DELETE FROM `game_event_creature`;
DELETE FROM `game_event_creature_quest`;
DELETE FROM `game_event_gameobject`;
DELETE FROM `game_event_gameobject_quest`;
DELETE FROM `game_event_model_equip`;
DELETE FROM `game_event_npc_vendor`;
DELETE FROM `game_event_npcflag`;
DELETE FROM `game_event_pool`;
DELETE FROM `game_event_prerequisite`;
DELETE FROM `game_event_quest_condition`;
DELETE FROM `game_event_seasonal_questrelation`;
INSERT INTO `game_event` (`eventEntry`, `start_time`, `end_time`, `occurence`, `length`, `holiday`, `holidayStage`, `WorldStateId`, `description`, `world_event`, `announce`) VALUES
(1, '2020-06-21 20:00:00', '2037-12-31 23:59:59', 525600, 20160, 341, 0, NULL, 'Midsummer Fire Festival', 0, 0),
(2, '2020-12-16 23:00:00', '2037-12-31 23:59:59', 525600, 25980, 141, 0, NULL, 'Feast of Winter Veil', 0, 0),
(4, NULL, NULL, 525600, 2592000, 374, 0, NULL, 'Darkmoon Faire (Elwynn)', 1, 0),
(5, NULL, NULL, 525600, 2592000, 375, 0, NULL, 'Darkmoon Faire (Mulgore)', 1, 0),
(6, '2017-01-01 04:00:00', '2037-01-01 00:00:00', 525600, 10, 0, 0, NULL, 'Fireworks', 1, 0),
(7, '2020-01-24 22:00:00', '2037-12-31 23:59:59', 525600, 20160, 327, 0, NULL, 'Lunar Festival', 0, 0),
(8, '2021-02-11 22:00:00', '2037-02-11 23:00:00', 525600, 7200, 335, 0, NULL, 'Love is in the Air', 0, 0),
(10, '2020-05-01 20:00:00', '2037-12-31 23:59:59', 525600, 10080, 201, 0, NULL, 'Children\'s Week', 0, 0),
(11, '2020-09-29 22:00:00', '2037-12-31 23:59:59', 525600, 10080, 321, 0, NULL, 'Harvest Festival', 0, 0),
(12, '2020-10-18 20:00:00', '2037-12-31 23:59:59', 525600, 20160, 324, 0, NULL, 'Hallow\'s End', 0, 0),
(13, NULL, NULL, 1, 999999999, 0, 0, NULL, 'Elemental Invasion', 1, 0),
(14, '2017-01-07 09:00:00', '2037-01-01 00:00:00', 10080, 1920, 0, 0, NULL, 'Stranglethorn Fishing Extravaganza: Announce', 0, 0),
(15, '2017-01-08 14:00:00', '2037-01-01 00:00:00', 10080, 120, 301, 0, NULL, 'Stranglethorn Fishing Extravaganza: Tournament', 0, 0),
(16, '2020-12-31 23:00:00', '2037-12-31 23:59:59', 180, 120, 0, 0, NULL, 'Gurubashi Arena Booty Run', 0, 0),
(18, '2020-05-01 00:00:01', '2037-12-31 23:59:59', 30240, 6240, 283, 0, NULL, 'Call to Arms: Alterac Valley', 0, 0),
(19, '2020-05-08 00:00:01', '2037-12-31 23:59:59', 30240, 6240, 284, 0, NULL, 'Call to Arms, Warsong Gulch', 0, 0),
(20, '2020-05-15 00:00:01', '2037-12-31 23:59:59', 30240, 6240, 285, 0, NULL, 'Call to Arms, Arathi Basin', 0, 0),
(21, '2020-12-25 00:00:00', '2037-01-03 00:59:59', 525600, 12960, 0, 0, NULL, 'Feast of Winter Veil: Gifts', 0, 0),
(23, NULL, NULL, 525600, 2592000, 374, 0, NULL, 'Darkmoon Faire Building (Elwynn)', 1, 0),
(24, NULL, NULL, 525600, 2592000, 375, 0, NULL, 'Darkmoon Faire Building (Mulgore)', 1, 0),
(25, NULL, NULL, 525600, 1, 0, 0, NULL, 'Call to Arms', 0, 0),
(27, '2020-01-02 21:00:00', '2037-12-31 09:00:00', 1440, 720, 0, 0, NULL, 'Night', 0, 0),
(28, NULL, NULL, 525600, 2880, 0, 0, NULL, 'Noblegarden', 0, 0),
(29, '2020-06-10 00:00:01', '2037-12-31 23:59:59', 80640, 20160, 0, 0, NULL, 'Edge of Madness, Gri\'lek', 0, 0),
(30, '2020-06-23 00:00:01', '2037-12-31 23:59:59', 80640, 20160, 0, 0, NULL, 'Edge of Madness, Hazza\'rah', 0, 0),
(31, '2020-05-12 00:00:01', '2037-12-31 23:59:59', 80640, 20160, 0, 0, NULL, 'Edge of Madness, Renataki', 0, 0),
(32, '2020-05-26 00:00:01', '2037-12-31 23:59:59', 80640, 20160, 0, 0, NULL, 'Edge of Madness, Wushoolay', 0, 0),
(33, NULL, NULL, 5184000, 2592000, 0, 0, NULL, 'Arena Tournament', 0, 0),
(34, '2020-12-31 06:00:00', '2037-01-01 07:00:05', 525600, 1445, 0, 0, NULL, 'New Year\'s Eve', 0, 0),
(35, '2010-03-24 11:00:00', '2037-01-01 05:00:00', 1260, 420, 0, 0, NULL, 'The Maul: Mushgog', 0, 0),
(36, '2010-03-24 18:00:00', '2037-01-01 05:00:00', 1260, 420, 0, 0, NULL, 'The Maul: Skarr the Unbreakable', 0, 0),
(37, '2010-03-25 01:00:00', '2037-01-01 05:00:00', 1260, 420, 0, 0, NULL, 'The Maul: Razza', 0, 0),
(38, '2020-02-08 06:00:00', '2037-02-09 07:00:00', 525600, 1445, 0, 0, NULL, 'Chinese New Year', 0, 0),
(39, NULL, NULL, 60, 10, 0, 0, NULL, 'Post Firework Toasting Goblets', 1, 0),
(40, '2017-01-08 12:00:00', '2037-01-01 00:00:00', 10080, 300, 0, 0, NULL, 'Stranglethorn Fishing Extravaganza: The Crew', 0, 0),
(41, '2020-06-04 00:00:00', '2037-06-05 02:00:00', 525600, 1440, 0, 0, NULL, 'July 4th Fireworks Spectacular (US Only)', 0, 0),
(42, '2020-09-30 00:00:00', '2037-09-30 02:00:00', 525600, 1440, 0, 0, NULL, 'September 30th Peon Day (EU Only)', 0, 0),
(43, NULL, NULL, 1, 999999999, 0, 0, NULL, 'Lunar Festival: Minions of Omen', 1, 0),
(45, '2020-03-20 10:00:00', '2037-01-01 05:00:00', 525600, 262800, 0, 0, NULL, 'Fishing: summer period (for Raw Summer Bass)', 0, 0),
(46, '2020-09-23 09:00:00', '2037-01-01 05:00:00', 525600, 262800, 0, 0, NULL, 'Fishing: winter period (for Winter Squid)', 0, 0),
(47, '2017-01-01 06:00:00', '2037-12-31 01:00:00', 1440, 1080, 0, 0, NULL, 'Fishing: Sunscale Cycle', 0, 0),
(48, '2017-01-01 18:00:00', '2037-12-31 13:00:00', 1440, 1080, 0, 0, NULL, 'Fishing: Nightfin Cycle', 0, 0),
(49, '2020-01-02 21:00:00', '2037-12-31 06:30:00', 1440, 480, 0, 0, NULL, 'Pyrewood Village Night', 0, 0),
(66, NULL, NULL, 1, 999999999, 0, 0, NULL, 'Dragons of Nightmare Spawn', 1, 0),
(68, NULL, NULL, 1, 999999999, 0, 0, NULL, 'Elemental Invasion: Fire (Un\'goro Crater)', 1, 0),
(69, NULL, NULL, 1, 999999999, 0, 0, NULL, 'Elemental Invasion: Air (Silithus)', 1, 0),
(70, NULL, NULL, 1, 999999999, 0, 0, NULL, 'Elemental Invasion: Earth (Azshara)', 1, 0),
(71, NULL, NULL, 1, 999999999, 0, 0, NULL, 'Elemental Invasion: Water (Winterspring)', 1, 0),
(72, NULL, NULL, 1, 999999999, 0, 0, NULL, 'Elemental Invasion: Baron Charr (Un\'goro Crater)', 1, 0),
(73, NULL, NULL, 1, 999999999, 0, 0, NULL, 'Elemental Invasion: The Windreaver (Silithus)', 1, 0),
(74, NULL, NULL, 1, 999999999, 0, 0, NULL, 'Elemental Invasion: Avalanchion (Azshara)', 1, 0),
(75, NULL, NULL, 1, 999999999, 0, 0, NULL, 'Elemental Invasion: Princess Tempestria (Winterspring)', 1, 0),
(78, '2010-01-01 01:00:00', '2037-01-01 00:00:00', 60, 1, 0, 0, NULL, 'Hourly Bells', 0, 0),
(81, NULL, '2037-01-01 00:59:59', 525600, 30240, 0, 0, NULL, 'Scourge Invasion - Boss in instance activation', 0, 0),
(84, '2005-01-01 00:00:00', '2037-01-01 00:00:00', 1, 2, 0, 0, NULL, 'AQ War Effort (master event)', 1, 0),
(85, NULL, NULL, 1, 999999999, 0, 0, NULL, 'AQ War Effort (gong)', 1, 0),
(86, NULL, NULL, 1, 2, 0, 0, NULL, 'AQ War Effort (post-event)', 1, 0),
(103, '2020-09-08 04:00:00', '2037-12-31 04:00:00', 525600, 33720, 0, 0, NULL, 'Event PVP Silithus', 0, 0),
(129, NULL, '2037-01-01 00:59:59', 525600, 30240, 0, 0, NULL, 'Scourge Invasion - Phase 2 - Invasion Stormwind', 0, 0),
(130, NULL, '2037-01-01 00:59:59', 525600, 30240, 0, 0, NULL, 'Scourge Invasion - Phase 2 - Invasion Undercity', 0, 0),
(127, '2022-01-01 00:19:48', '2037-03-31 03:19:48', 120, 35, 0, 0, NULL, 'Southshore Assassins', 0, 0);
