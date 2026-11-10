-- Classic 1.60 (Forever): Anachronos (15192): faction 776 is the Brood of Nozdormu, which every race starts at Hated (Faction.db2 ReputationBase -42000),
--    so he attacked everyone who came to him in Tanaris. He hands out the Scepter of the Shifting Sands chain and nobody should
--    fight him: friendly to both sides. His quests check Brood of Nozdormu reputation on the player, not his faction.
UPDATE `creature_template` SET `faction`=35 WHERE `entry`=15192;

-- Archmage Celindra (252085, Violet Citadel): turn-in for Friend of the Kirin Tor (Alliance) and Shrewd Negotiations (Horde). Faction
--    3562 (Kirin Tor) is hostile to the Horde, so Horde players could not hand in. Wowhead Forever shows her friendly to the Alliance
--    and neutral to the Horde; FactionTemplate 3591 is the only template with exactly that reaction (no reputation faction).
UPDATE `creature_template` SET `faction`=3591 WHERE `entry`=252085;
