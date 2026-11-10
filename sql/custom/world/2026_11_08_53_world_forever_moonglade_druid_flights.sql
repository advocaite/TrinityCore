-- Moonglade druid flights (Discord, crilico, 2026-11-08): Silva Fil'naveth (11800) "I'd like to fly to Rut'theran Village."
-- did nothing for a night elf druid on Back to Darnassus. Bunthen Plainswind (11798, to Thunder Bluff) uses the same script.
--
-- Checked and correct: menu 4041/4042 option 0 and its Alliance/Horde druid conditions, AIName SmartAI, the gossip select
-- events, spells 27998/28001 in the 70862 client data (Send Taxi, taxi paths 315 Nighthaven -> Rut'theran and
-- 316 Nighthaven -> Thunder Bluff, 31 and 78 path nodes, no cooldown, no area or aura requirement).
--
-- The flight master herself cast the flight spell on the player, a normal (not triggered) cast: it can fail on her side
-- without any message (range of 5 yards from her, line of sight on the roost, a spell already in progress), and then
-- only the gossip window closes. Now the player casts the flight spell on their own character, triggered, like the scripted
-- flight of Greer Orehammer, so nothing on the flight master's side can stop it. Safe to run again.

UPDATE `smart_scripts` SET `action_type` = 134, `action_param2` = 2,  -- SMART_ACTION_INVOKER_CAST, SMARTCAST_TRIGGERED
    `target_type` = 7,                                                -- SMART_TARGET_ACTION_INVOKER (the player is the target)
    `comment` = 'Silva Fil''naveth - On Gossip Option 0 Selected - Invoker Cast ''Flight Path'' (to Rut''theran Village)'
WHERE `entryorguid` = 11800 AND `source_type` = 0 AND `id` = 0 AND `event_type` = 62 AND `action_param1` = 27998;

UPDATE `smart_scripts` SET `action_type` = 134, `action_param2` = 2,
    `target_type` = 7,
    `comment` = 'Bunthen Plainswind - On Gossip Option 0 Selected - Invoker Cast ''Flight Path'' (to Thunder Bluff)'
WHERE `entryorguid` = 11798 AND `source_type` = 0 AND `id` = 0 AND `event_type` = 62 AND `action_param1` = 28001;
