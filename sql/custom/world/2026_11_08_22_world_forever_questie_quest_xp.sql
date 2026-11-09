-- Quest levels and XP from the Questie project's quest traces (Questie team, Questie/QuestieDB,
-- support/Forever/QuestXP/xpDB-traces.lua, master 3a621efa, 2026-11-08, GPLv3): the level and XP that players got from
-- 724 Forever quests in the live game. Compared with ours through the client's QuestXP table (1.15 / 1.60 columns).
-- 493 already match in XP. Changed here:
-- 1) Quest level: the level the live game sends for the quest (the XP and gray-quest level), where ours differs.
-- 2) XP: only where the live value is higher than ours. A lower live value is left alone: players above the quest's level
--    get 80/60/40/20/10% of it, so a low trace can be that cut and not the quest's real XP.
--    The difficulty column is the one that gives the live XP exactly at the live level; multiplier 1.
-- Safe to run again.

-- 1) quest level (ours -> live)
UPDATE `quest_classic_level` SET `QuestLevel`=27 WHERE `ID`=65610; -- Wish You Were Here: 20 -> 27
UPDATE `quest_classic_level` SET `QuestLevel`=27 WHERE `ID`=79363; -- Silvia's Sword: 26 -> 27
UPDATE `quest_classic_level` SET `QuestLevel`=10 WHERE `ID`=86667; -- Snowbound: 20 -> 10
UPDATE `quest_classic_level` SET `QuestLevel`=30 WHERE `ID`=86757; -- Slowing the Orc Advance: 32 -> 30
UPDATE `quest_classic_level` SET `QuestLevel`=8 WHERE `ID`=91723; -- Delicate Instruments: 10 -> 8
UPDATE `quest_classic_level` SET `QuestLevel`=7 WHERE `ID`=91732; -- Good Steel: 10 -> 7
UPDATE `quest_classic_level` SET `QuestLevel`=11 WHERE `ID`=91740; -- Croaky's Head: 10 -> 11
UPDATE `quest_classic_level` SET `QuestLevel`=12 WHERE `ID`=91746; -- Elmpaw's Head: 10 -> 12
UPDATE `quest_classic_level` SET `QuestLevel`=8 WHERE `ID`=91751; -- Rough Wolf Pelts: 7 -> 8
UPDATE `quest_classic_level` SET `QuestLevel`=15 WHERE `ID`=92747; -- Moonbrook Espionage: 16 -> 15
UPDATE `quest_classic_level` SET `QuestLevel`=13 WHERE `ID`=95328; -- Whispering Horror Residue: 10 -> 13
UPDATE `quest_classic_level` SET `QuestLevel`=5 WHERE `ID`=96130; -- Chakuyak: 8 -> 5
UPDATE `quest_classic_level` SET `QuestLevel`=25 WHERE `ID`=96137; -- Ira's Dagger: 24 -> 25
UPDATE `quest_classic_level` SET `QuestLevel`=30 WHERE `ID`=96138; -- Merrick's Bow: 28 -> 30
UPDATE `quest_classic_level` SET `QuestLevel`=23 WHERE `ID`=96139; -- The Valor Family: 22 -> 23
UPDATE `quest_classic_level` SET `QuestLevel`=29 WHERE `ID`=96241; -- Talkin' bout Toxins, Mon: 30 -> 29
UPDATE `quest_classic_level` SET `QuestLevel`=4 WHERE `ID`=96243; -- Call of Earth: 10 -> 4
UPDATE `quest_classic_level` SET `QuestLevel`=29 WHERE `ID`=96259; -- Defending the Dead: 30 -> 29
UPDATE `quest_classic_level` SET `QuestLevel`=29 WHERE `ID`=96260; -- Stronger than Steel: 30 -> 29
UPDATE `quest_classic_level` SET `QuestLevel`=7 WHERE `ID`=96822; -- For Honor: 6 -> 7
UPDATE `quest_classic_level` SET `QuestLevel`=12 WHERE `ID`=96896; -- A Righteous Cause: 13 -> 12
UPDATE `quest_classic_level` SET `QuestLevel`=9 WHERE `ID`=97223; -- Bloodtalon Matriarch: 8 -> 9
UPDATE `quest_classic_level` SET `QuestLevel`=15 WHERE `ID`=97253; -- Parts and Pieces: 14 -> 15
UPDATE `quest_classic_level` SET `QuestLevel`=10 WHERE `ID`=97282; -- Stormy Potential: 11 -> 10
UPDATE `quest_classic_level` SET `QuestLevel`=30 WHERE `ID`=98070; -- Stop the Screeching: 31 -> 30
UPDATE `quest_classic_level` SET `QuestLevel`=29 WHERE `ID`=98155; -- Pristine Pesterhide Pelts: 31 -> 29
UPDATE `quest_classic_level` SET `QuestLevel`=23 WHERE `ID`=98251; -- Never Coming Back: 22 -> 23
UPDATE `quest_classic_level` SET `QuestLevel`=12 WHERE `ID`=98545; -- Leonid's Letter: 13 -> 12
UPDATE `quest_classic_level` SET `QuestLevel`=9 WHERE `ID`=99048; -- A Missing Hand: 11 -> 9
UPDATE `quest_classic_level` SET `QuestLevel`=9 WHERE `ID`=99049; -- Threat from Below: 11 -> 9
UPDATE `quest_classic_level` SET `QuestLevel`=11 WHERE `ID`=99052; -- Threat from Below: 12 -> 11
UPDATE `quest_classic_level` SET `QuestLevel`=10 WHERE `ID`=99053; -- Escaping Ban'ethil: 9 -> 10
UPDATE `quest_classic_level` SET `QuestLevel`=11 WHERE `ID`=99080; -- Drive Them Out: 9 -> 11
UPDATE `quest_classic_level` SET `QuestLevel`=11 WHERE `ID`=99082; -- The High Chieftain: 9 -> 11
UPDATE `quest_classic_level` SET `QuestLevel`=12 WHERE `ID`=99152; -- As Above, So Below: 10 -> 12
UPDATE `quest_classic_level` SET `QuestLevel`=12 WHERE `ID`=99153; -- The One That Got Away: 10 -> 12

-- 2) XP (ours -> live)
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=87760; -- Holy Diver: 0 -> 1550 (column 0 -> 5)
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=91751; -- Rough Wolf Pelts: 475 -> 700 (column 4 -> 5)
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=91772; -- Shhh! We're Hunting Kobolds: 270 -> 355 (column 4 -> 5)
UPDATE `quest_template` SET `RewardXPDifficulty`=4, `RewardXPMultiplier`=1 WHERE `ID`=92747; -- Moonbrook Espionage: 580 -> 800 (column 3 -> 4)
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=92911; -- Harvesting the Harvesters: 800 -> 1050 (column 4 -> 5)
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=95508; -- Unwelcome Guests: 1000 -> 1350 (column 4 -> 5)
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=95737; -- Seeking Caitlin: 630 -> 1250 (column 2 -> 3)
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=95805; -- Grace of An'she and Mu'sha: 180 -> 355 (column 3 -> 5)
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=95810; -- Lost Relic Carry: 1228 -> 1250 (column 2 -> 3, multiplier 1.95 -> 1)
UPDATE `quest_template` SET `RewardXPDifficulty`=4, `RewardXPMultiplier`=1 WHERE `ID`=96130; -- Chakuyak: 350 -> 335 (column 3 -> 4)
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=96821; -- Legging It: 405 -> 540 (column 4 -> 5)
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=96825; -- This Fruit Could Bite Back: 405 -> 540 (column 4 -> 5)
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=96874; -- This Is Spinal Axe: 630 -> 840 (column 4 -> 5)
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=96875; -- Beasts of Thunder Ridge: 630 -> 840 (column 4 -> 5)
UPDATE `quest_template` SET `RewardXPDifficulty`=6, `RewardXPMultiplier`=1 WHERE `ID`=96876; -- Ukor's Lost Pack: 530 -> 880 (column 4 -> 6)
UPDATE `quest_template` SET `RewardXPDifficulty`=6, `RewardXPMultiplier`=1 WHERE `ID`=96877; -- Halikor's Hoof: 680 -> 1150 (column 4 -> 6)
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=97894; -- Business in Auberdine: 0 -> 630 (column 0 -> 3)
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=97914; -- Expanding Horizons: 125 -> 630 (column 1 -> 3)
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=97926; -- Making Do: 125 -> 630 (column 1 -> 3)
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=98191; -- A Friend of the Family: 200 -> 2000 (column 1 -> 5)
UPDATE `quest_template` SET `RewardXPDifficulty`=3, `RewardXPMultiplier`=1 WHERE `ID`=98319; -- Secure the Mountain: 175 -> 350 (column 2 -> 3)
UPDATE `quest_template` SET `RewardXPDifficulty`=6, `RewardXPMultiplier`=1 WHERE `ID`=99052; -- Threat from Below: 910 -> 1100 (column 5 -> 6)
UPDATE `quest_template` SET `RewardXPDifficulty`=5, `RewardXPMultiplier`=1 WHERE `ID`=99053; -- Escaping Ban'ethil: 0 -> 840 (column 0 -> 5)
