-- Classic 1.60 client 70532: the class spells, talents and pet abilities last as long as the 70532 data and their tooltips say, not the vanilla
-- duration classic_spell_info gave them (Shaman totems 1-2 minutes instead of 5, Judgements 10 seconds instead of 40, Demoralizing Shout 30 seconds
-- instead of 45 and so on). Only the duration goes: a row that also changes the cast time, cooldown or range keeps that. Left alone: spells the client
-- gives no duration (Omen of Clarity, Judgement of Fury), the channels made from Forever twins (Drain Life, Mind Flay), items and creature spells.
-- Safe to run again.
UPDATE `classic_spell_info` SET `duration_ms` = NULL WHERE `spell_id` IN (
-- Hunter: Eyes of the Beast, Serpent Sting (Forever twins), pet Screech and Furious Howl
1002, 425728, 425729, 425730, 425732, 425733, 425734, 425735, 425736, 425737, 24423, 24577, 24578, 24579, 24597, 24603, 24604, 24605,
-- Mage: Cone of Cold
120, 8492, 10159, 10160, 10161,
-- Paladin: Judgement of Light, Wisdom and the Crusader, Vengeance
20185, 20344, 20345, 20346, 20186, 20354, 20355, 21183, 20188, 20300, 20301, 20302, 20303, 25942, 20050,
-- Priest: Fear Ward, Vampiric Embrace, Spirit of Redemption, Feedback (Forever twin)
6346, 15286, 27827, 447549,
-- Druid: Natures Grace
16886,
-- Rogue: Premeditation
14183,
-- Shaman: totems (Disease Cleansing, Fire/Frost/Nature Resistance, Flametongue, Grace of Air, Healing Stream, Mana Spring, Mana Tide, Poison Cleansing,
-- Stoneskin, Strength of Earth, Tremor, Windfury, Windwall)
8170, 8184, 10537, 10538, 8227, 8249, 10526, 16387, 8181, 10478, 10479, 8835, 10627, 25359, 5394, 6375, 6377, 10462, 10463, 5675, 10495, 10496, 10497,
16190, 17354, 17359, 10595, 10600, 10601, 8166, 8071, 8154, 8155, 10406, 10407, 10408, 8075, 8160, 8161, 10442, 25361, 8143, 8512, 10613, 10614,
15107, 15111, 15112,
-- Warlock: Shadowburn, Demonic Sacrifice buffs, Curse of Tongues and Banish (Forever twins)
17877, 18867, 18868, 18869, 18870, 18871, 18789, 18790, 18791, 18792, 402794, 465352,
-- Warrior: Demoralizing Shout, Shield Block, Shield Wall
1160, 6190, 11554, 11555, 11556, 2565, 871,
-- Undead: Will of the Forsaken
7744);
DELETE FROM `classic_spell_info` WHERE `duration_ms` IS NULL AND `cast_ms` IS NULL AND `recovery_ms` IS NULL AND `category_recovery_ms` IS NULL
    AND `range_max` IS NULL AND `radius1` IS NULL AND `radius2` IS NULL AND `radius3` IS NULL;
