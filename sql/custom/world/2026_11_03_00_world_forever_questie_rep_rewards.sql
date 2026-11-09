-- Quest reputation rewards and one exclusive pair, cross-checked with Questie/QuestieDB
-- (Questie team, Questie/QuestieDB, GPLv3) Forever traces + foreverQuestFixes.lua, master 31cc4af5 (2026-11-02).
-- Our own SQL: only the facts are used. Spillover (Steamwheedle towns, faction-wide gains) is left to the
-- server's reputation_spillover_template, so only the direct faction of each quest is set here.
-- Safe to run twice: each row is set only where the quest has no reward faction yet (RewardFactionID1 = 0).
-- Applied twice on a scratch world database (4 Nov). RewardFactionOverride = reputation x 100.

-- 91753 (Tower of Azora, Kitta Firewind): Stormwind +75
UPDATE `quest_template` SET `RewardFactionID1`=72, `RewardFactionOverride1`=7500
 WHERE `ID`=91753 AND `RewardFactionID1`=0;

-- 92706 Bruuz's Dorsal Fin (Gazlowe, Ratchet): Ratchet +100. Questie also shows Booty Bay, Gadgetzan,
-- Everlook +50 and 169/2827 +50: that is Steamwheedle spillover (half), not set here.
UPDATE `quest_template` SET `RewardFactionID1`=470, `RewardFactionOverride1`=10000
 WHERE `ID`=92706 AND `RewardFactionID1`=0;

-- 95140 Old Fire-Eye (Silverpine): 2787 +150. Thunder Bluff / Darkspear +56 look like spillover from 2787
-- (0.375 of 150), INFERRED, not set here.
UPDATE `quest_template` SET `RewardFactionID1`=2787, `RewardFactionOverride1`=15000
 WHERE `ID`=95140 AND `RewardFactionID1`=0;

-- 99048 (find Heglan Shadeeye north of Tiragarde Keep): Orgrimmar +25. Questie also shows Darkspear +25;
-- not set until checked in game (it may be spillover).
UPDATE `quest_template` SET `RewardFactionID1`=76, `RewardFactionOverride1`=2500
 WHERE `ID`=99048 AND `RewardFactionID1`=0;

-- 971 Knowledge in the Deeps is closed once 968 is done (Questie disabledByQuest 968).
-- Positive ExclusiveGroup = only one quest of the group can be done. INFERRED as a two-way pair.
INSERT IGNORE INTO `quest_template_addon` (`ID`) VALUES (968),(971);
UPDATE `quest_template_addon` SET `ExclusiveGroup`=968 WHERE `ID` IN (968,971) AND `ExclusiveGroup`=0;
