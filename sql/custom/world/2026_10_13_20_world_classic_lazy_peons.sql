-- Classic 1.60: Lazy Peons (quest 5441). Waking a sleeping Lazy Peon (10556) with the Foreman's Blackjack (Awaken Peon 19938)
-- is credited by npc_lazy_peon, but the creature had no script name, so no credit was given.
UPDATE `creature_template` SET `ScriptName` = 'npc_lazy_peon' WHERE `entry` = 10556;
