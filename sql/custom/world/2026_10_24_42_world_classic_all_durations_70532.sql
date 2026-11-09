-- Classic 1.60 client 70532: every other spell takes its duration from the 70532 data too (asked 2026-10-24: "put all the buffs and the timers to forever").
-- The protection potions last 2 minutes instead of 1 hour, Elixir of the Mongoose 30 minutes instead of 1 hour, and the weapon procs and creature spells
-- their 70532 time. Only the duration goes: a row that also changes the cast time, cooldown or range keeps that. Left alone: spells the client has no
-- duration for or holds forever (Omen of Clarity, Bestial Wrath 24395-24397, Frost Aura and the like), where the server value is the only one.
-- Drain Life 403677-403689: the 70532 spell is a 15 s drain that is not channeled, with a 15 s cooldown. It stops being a twin of the vanilla channel
-- (LoadClassicTwinChannels) and loses the vanilla 5 s and no cooldown. The Drain Life the trainers teach (689-11700) is a 5 s channel in 70532 too and
-- keeps it. Safe to run again.
UPDATE `classic_spell_info` SET `duration_ms` = NULL WHERE `spell_id` IN (
-- Protection potions (Fire, Frost, Shadow, Holy, Nature, Arcane), Elixir of the Mongoose, Ragged John's Neverending Cup
7230, 7231, 7232, 7233, 7234, 7235, 7236, 7237, 7238, 7239, 7240, 7241, 7242, 7243, 7244, 7245, 7246, 7247, 7248, 7249, 7250, 7251, 7252, 7253, 7254,
17543, 17544, 17545, 17546, 17548, 17549, 17538, 20587,
-- Weapon and trinket procs: Lord General's Sword, Demonfork, Seeping Willow, Skullforge Brand, Soul Breaker, Surge of Strength, Wound, Windreaper,
-- Eskhandar's Rake and Rage, Devilsaur Fury, Defender of the Timbermaw, Corrosive Poison, Poison, Creeping Mold, Weakening Disease, Siphon Health, Stun
15602, 16603, 17196, 17484, 17506, 17499, 17407, 20586, 22639, 22640, 24352, 26066, 13526, 8313, 17330, 18203, 21952, 18289, 18633, 18652, 17308,
-- Windfury Totem attack buff
8516, 10608, 10610,
-- Mind Flay (Forever twins)
474204, 474268,
-- Creature spells: Rend, Poison Cloud, Fireball, Corruption, Demoralizing Shout, Static Barrier, Entangling Roots, Sleep, Pyroblast, Earthquake, the
-- Silithus Templar, Duke and Royal summons, Summon Felhunter, Summon Tracking Hound, Summon Holy Mount Visual
14118, 18075, 18200, 18202, 21949, 11790, 12766, 16413, 18656, 23511, 11841, 435991, 450662, 460756, 462058, 460858, 424, 24734, 24744, 24756, 24758,
24760, 24763, 24765, 24768, 24770, 24772, 24784, 24786, 24788, 24789, 24790, 8176, 9515, 31726);
DELETE FROM `classic_spell_twin` WHERE `spell_id` IN (403677, 403685, 403686, 403687, 403688, 403689);
DELETE FROM `classic_spell_info` WHERE `spell_id` IN (403677, 403685, 403686, 403687, 403688, 403689);
DELETE FROM `classic_spell_cost` WHERE `spell_id` IN (403677, 403685, 403686, 403687, 403688, 403689);
DELETE FROM `classic_spell_value` WHERE `spell_id` IN (403677, 403685, 403686, 403687, 403688, 403689);
DELETE FROM `classic_spell_info` WHERE `duration_ms` IS NULL AND `cast_ms` IS NULL AND `recovery_ms` IS NULL AND `category_recovery_ms` IS NULL
    AND `range_max` IS NULL AND `radius1` IS NULL AND `radius2` IS NULL AND `radius3` IS NULL;
