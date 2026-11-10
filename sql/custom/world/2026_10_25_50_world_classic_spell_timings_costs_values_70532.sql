-- Classic 1.60 client 70532: class spells take their cast time, cooldown, range, mana cost and effect values from the 70532 data where
-- the vanilla rows of classic_spell_info, classic_spell_cost and classic_spell_value (VMaNGOS 1.12, generated) still overrode them. Found by the
-- 70532 client audit (tools/client_diff). Examples: Lay on Hands cooldown 1 hour -> 20 minutes, Lightning Bolt 3 s -> 2.5 s, Aimed Shot 3 s -> 2 s,
-- Holy Shock cooldown 30 s -> 10 s, Stormstrike 20 s -> 8 s, Slam 18 s cooldown, Regrowth and Wrath cheaper, Holy Strike 9-20 mana. A row keeps
-- the columns this file does not clear. Safe to run again.
-- Cast time
UPDATE `classic_spell_info` SET `cast_ms` = NULL WHERE `spell_id` IN (
-- Hunter: Aimed Shot
19434, 20900, 20901, 20902, 20903, 20904,
-- Companion: Summon Black Kingsnake
10714,
-- Companion: Summon Cockroach
10688,
-- Companion: Summon Great Horned Owl
10707,
-- Companion: Summon Orange Tabby
10676,
-- Companion: Summon Prairie Dog
10709,
-- Companion: Summon Snowshoe Rabbit
10711,
-- Shaman: Chain Lightning
421, 930, 2860, 10605, 408479, 408481, 408482, 408484,
-- Shaman: Lightning Bolt
915, 943, 6041, 10391, 10392, 15207, 15208, 408439, 408440, 408441, 408442, 408443, 408472, 408473, 408475, 408477,
-- Warlock: Summon Incubus
713);
-- Cooldown (the spell and the category cooldown)
UPDATE `classic_spell_info` SET `recovery_ms` = NULL, `category_recovery_ms` = NULL WHERE `spell_id` IN (
-- Druid: Growl
6795,
-- Druid: Hurricane
16914, 17401, 17402,
-- Druid: Tiger's Fury
5217,
-- Hunter: Explosive Trap
13813, 14316, 14317,
-- Hunter: Freezing Trap
1499, 14310, 14311,
-- Hunter: Frost Trap
13809,
-- Hunter: Immolation Trap
13795, 14302, 14303, 14304, 14305,
-- Hunter: Multi-Shot
2643,
-- Hunter: Raptor Strike
409691, 409693, 409748, 409750, 409751, 409752, 409754, 409755,
-- Hunter: Viper Sting
3034, 14279, 14280,
-- Hunter: Volley
1510, 14294, 14295,
-- Mage: Fire Blast
400616, 400618, 400619, 400620, 400621, 400622, 400623,
-- Companion: Summon Black Kingsnake
10714,
-- Companion: Summon Cockroach
10688,
-- Companion: Summon Great Horned Owl
10707,
-- Companion: Summon Orange Tabby
10676,
-- Companion: Summon Prairie Dog
10709,
-- Companion: Summon Snowshoe Rabbit
10711,
-- Paladin: Holy Shock
20473, 20929, 20930,
-- Paladin: Lay on Hands
633, 2800, 10310,
-- Priest: Devouring Plague
2944, 19276, 19277, 19278, 19279, 19280,
-- Priest: Fear Ward
6346,
-- Priest: Starshards
10797, 19296, 19299, 19302, 19303, 19304, 19305,
-- Priest: Vampiric Embrace
15286,
-- Shaman: Chain Lightning
408479, 408481, 408482, 408484,
-- Shaman: Stormstrike
17364,
-- Warlock: Drain Life
403677, 403685, 403686, 403687, 403688, 403689,
-- Warlock: Fel Domination
18708,
-- Warrior: Last Stand
12975,
-- Warrior: Retaliation
20230,
-- Warrior: Shield Wall
871,
-- Warrior: Slam
1464, 8820, 11604, 11605,
-- Warrior: Taunt
355,
-- Warrior: Thunder Clap
6343, 8198, 8204, 8205, 11580, 11581);
-- Range
UPDATE `classic_spell_info` SET `range_max` = NULL WHERE `spell_id` IN (
-- Hunter: Serpent Sting
425728, 425729, 425730, 425732, 425733, 425734, 425735, 425736, 425737,
-- Paladin: Holy Shock
25903, 25913, 25914,
-- Warlock: Shadowburn
17877, 18867, 18868, 18869, 18870, 18871);
DELETE FROM `classic_spell_info` WHERE `duration_ms` IS NULL AND `cast_ms` IS NULL AND `recovery_ms` IS NULL AND `category_recovery_ms` IS NULL
    AND `range_max` IS NULL AND `radius1` IS NULL AND `radius2` IS NULL AND `radius3` IS NULL;
-- Mana cost
DELETE FROM `classic_spell_cost` WHERE `spell_id` IN (
-- Druid: Healing Touch
5188, 5189, 6778, 8903, 9758, 9888, 9889, 25297,
-- Druid: Regrowth
8936, 8938, 8939, 8940, 8941, 9750, 9856, 9857, 9858,
-- Druid: Wrath
5176, 5177, 5178, 5179, 5180, 6780, 8905, 9912,
-- Hunter: Counterattack
19306,
-- Hunter: Lacerate
24118, 24119, 24120,
-- Hunter: Multi-Shot
2643,
-- Hunter: Raptor Strike
2973,
-- Hunter: Scorpid Sting
3043,
-- Mage: Conjure Water
468766,
-- Paladin: Holy Strike
678, 679, 680, 1866, 2495, 5569, 10332, 10333,
-- Paladin: Seal of Fury
20163, 20419, 20421, 20422, 20423,
-- Priest: Elune's Grace
2651,
-- Priest: Feedback
13896, 19271, 19273, 19274, 19275,
-- Shaman: Chain Lightning
421, 930, 2860, 10605,
-- Shaman: Lightning Bolt
915, 943, 6041, 10391, 10392, 15207, 15208,
-- Shaman: Mana Tide Totem
16190, 17354, 17359,
-- Shaman: Stormstrike
17364,
-- Warlock: Death Coil
6789, 17925, 17926,
-- Warlock: Drain Life
403677, 403685, 403686, 403687, 403688, 403689);
-- Effect values on effects 70532 changed (Curse of Recklessness, Fire Power, Improved Seals and Unholy Shield were reworked and their rows no longer
-- matched; Shred, Holy Fire, Judgement and Seal of Righteousness, the Fire Nova twins and the item and creature spells take the 70532 numbers). The
-- Seal of Righteousness script still sets the damage from the weapon; only the spell power factor of ranks 1-3 changes.
DELETE FROM `classic_spell_value` WHERE (`spell_id`, `effect_index`) IN (
-- Druid: Shred
(5221, 0),
-- Druid: Wrath
(18104, 0),
-- Item and creature spells: Aura of Rot
(25819, 0),
-- Item and creature spells: Brain Hacker
(17148, 1),
-- Item and creature spells: Lash of Pain
(21987, 0),
-- Item and creature spells: Lily Root
(18832, 0), (18832, 1),
-- Item and creature spells: Minor Stats
(13624, 0),
-- Item and creature spells: Rend
(17153, 0), (17504, 0),
-- Item and creature spells: Shadow Bolt
(18211, 0),
-- Item and creature spells: Unholy Shield
(8909, 2),
-- Item and creature spells: Wound
(18107, 0),
-- Mage: Fire Power
(11124, 0), (11124, 1),
-- Mage: Fireball
(16415, 0), (16415, 1), (18796, 0), (18796, 1),
-- Mage: Firebolt
(18833, 0),
-- Paladin: Improved Seals
(20224, 0),
-- Paladin: Judgement of Righteousness
(20187, 0), (20280, 0), (20281, 0), (20282, 0), (20283, 0), (20284, 0), (20285, 0), (20286, 0),
-- Paladin: Seal of Righteousness
(25713, 0), (25735, 0), (25736, 0), (25737, 0), (25738, 0), (25739, 0), (25740, 0), (25741, 0), (25742, 0),
-- Priest: Holy Fire
(15265, 1),
-- Shaman: Fire Nova
(408423, 0), (408424, 0), (408426, 0), (408427, 0), (408428, 0),
-- Warlock: Curse of Recklessness
(704, 0), (7658, 0), (7659, 0), (11717, 0));
