-- Classic 1.60 (WoW Forever): Summon Felsteed (quest 4490) rewards spell 1710, which the 70532 client does not have (server audit
-- 2026-10-24, finding 8). The warlock finished the quest and learned nothing. The client has 5785 "Teach Summon Felsteed" (the learn
-- spell for 5784 Summon Felsteed, which the quest already shows as RewardDisplaySpell1), so the reward casts that.

UPDATE `quest_template` SET `RewardSpell` = 5785 WHERE `ID` = 4490 AND `RewardSpell` = 1710;
