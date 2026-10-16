# Raid layer (branch playerbot-v2-raid)

Port of Karrz's raid10-revamp ike3 raid logic (Karrz1/wow-1.12, src/game/PlayerBots) onto PlayerbotV2.

- `RaidEncounters.h/.cpp`: rule vocabulary and per-map registry (409 MC, 469 BWL, 249 Onyxia, 531 AQ40, 533 Naxx).
- `RaidEncounterData_*.cpp`: data only. MC/BWL/Onyxia ported as-is (ids match Forever's Classic VMaNGOS ports);
  Naxx is new (no ike3 source).
- `RaidBrain.cpp`: world thread, every 300 ms, one plan per (group, raid map), right after the PvE coordinator.
  Hazard moves, carrier runs, spread, cones, Deep Breath corridors, role/charge positions, elected taunt swaps,
  dispels (real dispel type read from spell data), purges, crowd control, forbidden targets, tank-first threat hold.
- `RaidOrder.h`: per-bot result, copied into `BotSnapshot::raid_order` by the builder.
- `RaidCombat.cpp`: consumed first in `State_InCombat` (`raid:*` rule names in /history).
- `RaidAdvice.cpp`: the raid maps' DungeonScripts take kill order + boss list from the rules and drop their
  generic movement/dispel/CC fields (the brain owns those).
- `RaidMechanics_Naxx.cpp` (+ `RaidBrainInternal.h`): scripted Naxx mechanics the tables cannot express:
  Heigan's eruption dance (counts the eruption markers, stands in the section the next eruption spares),
  Sapphiron's air phase (spread for Icebolt, hide behind the nearest ice block during Frost Breath),
  Razuvious (a priest bot mind-controls an understudy that taunts and tanks him, Shield Wall when low, a second
  priest takes over before the control runs out; raid tanks keep the free understudies).
- Generic rules added for Naxx: tank swaps walk the next clean tank over when it is out of taunt range (Four
  Horsemen rotation at 3 marks), `MeleeSoak` (Patchwerk: 3 off tanks stay on him in melee).
- No-AoE: `BotSnapshotView::enemies_within` reports at most 1 enemy while `raid_order.no_aoe` is set, which turns
  off every AoE gate in the rotations (Garr's Firesworn).
- Hooks outside this folder: BotSnapshot.h (field + reset), BotSnapshotBuilder.cpp (copy), PlayerbotV2.cpp
  (Update call), State_InCombat.cpp (dispatch), 4 raid DungeonScripts (one call each), BotSnapshotView.cpp
  (no-AoE clamp), core PlayerbotAPI.cpp (pet_attack / pet_cast drive a possessed creature when the bot has no pet).
- Config: `Playerbot.Raid.Enable` (default 1). Log: `[raid] <bot>: ...` lines at WARN on channel
  playerbot.v2.raid (shows with Logger.playerbot.v2=3), one per changed decision.

- Mind-controlled raiders (`ControlledMember`: Lucifron's Protectors, Grethok in BWL): nobody attacks them, a mage
  polymorphs one, otherwise a paladin / rogue / bear stuns it; recomputed every plan, so it ends with the aura.
- Boss tank swaps hand the boss over in the PvE coordinator's per-tank assignment (`ReassignTankTarget`).

Known AQ40 gaps: Viscidus' freeze/shatter, Kri's death cloud, C'Thun's Dark Glare and stomach phase.
- Naxx review (2026-10-06): Faerlina's worshipper sacrifice (priest MC + Widow's Embrace after each Enrage), Heigan's
  Mana Burn range for mana users, Kel'Thuzad's Chains as a controlled member and at most 3 shackled Guardians
  (`CrowdControl.maxActive`), Gluth tranq + zombies first, trash dispels/tranqs/priorities.
- Gothik: the brain also plans out of combat now (`MechanicsPrePull`). While a member is in his room, the gate is open
  and he is not engaged, bots alternate (per role) between the live and the dead side so each side has 10+; the idle
  rule `idle:raid_prepull` (@991, above owner follow) walks them there and holds. In the fight, while the gate is
  closed, each bot only attacks enemies on its own side, and nobody attacks Gothik while he is immune on the balcony.
- Sapphiron's ice blocks come from the stock TC aura script `spell_sapphiron_icebolt` (Northrend/Naxxramas, bound to
  28522 in spell_script_names): it summons GO 181247 under a frozen, standing player. If the Classic spell data's
  effect 2 is not the periodic trigger it expects, the script fails to load (worldserver logs it at startup) and there
  is no cover; the bots then have nothing to hide behind.
- `RaidMechanics_BWL.cpp`: Razorgore phase 1. A ranged DPS bot (never a tank or healer, not one with Mind Exhaustion)
  walks to the Orb of Domination and uses it; while it controls Razorgore it drives him egg to egg with Destroy Egg
  19873 (core `pet_cast` walks a possessed creature into reach of a game-object target first). Nobody attacks
  Razorgore while eggs are left. A human on the orb is left alone.
- `HoldByAura` (Nefarian's priest call, Corrupted Healing 23401: healing priests pause their rotation) and
  `CancelAura` (paladin call: tanks and physical DPS click off Blessing of Protection while Nefarian is up).
- The generic dispel assignment (trial's ClassicUpkeep) leaves every aura the raid rules own alone
  (`Raid::OwnsAura`: dispel, controlledMember and the new `noDispel` list, e.g. Huhuran's Wyvern Sting).

Known BWL gaps: Chromaggus' Bronze affliction needs Hourglass Sand; the other class calls (warrior Berserk, druid cat
form, hunter bow, rogue paralyze) have no counter a bot could play. Untested: whether Forever's spell data lets a
possessed Razorgore cast Destroy Egg on an egg, and the Blessing of Protection ids the paladin call uses.

Not done: 10-player scaling (the owner chose 40 only). Untested in game: the Naxx scripted mechanics, in particular
whether a possessed creature follows MoveChase for a bot controller and whether the understudy template spells
(Taunt 29060, Shield Wall 29061) are castable from Forever's data.

## Status (2026-10-05, paused for the night)

- Branch head f93ecd4f, pushed, builds clean on Linux (-DBUILD_PLAYERBOT_V2=1). It includes trial 513e126b.
- Batch 1 (MC/BWL/Onyxia port, threat hold) and batch 2 (Garr no-AoE, Heigan, Sapphiron, Razuvious, Four Horsemen,
  Patchwerk) are untested in game. the owner's first MC trash pull ran on df756243 and wiped: one Molten Giant had no tank,
  which the trial thread is fixing in pvecoord (one mob per tank in raids).
- The raid layer logs a "[raid] <raid> group N: ... main tank <name>" header when it plans a fight, plus one line
  per changed bot decision.
- To resume: merge playerbot-v2-trial into this branch, rebuild (cmake configure first when trial adds .cpp files) and
  send the head to the coordinator. Razuvious mind control is the most uncertain piece (see "Untested" above).

- Boss warnings (`BossWarnings.h/.cpp`, data in `BossWarningData_MC.h`): human raid members get an on-screen boss emote when a
  Molten Core boss starts a cast-bar spell ("Shazzrah is casting Arcane Explosion") or an instant ability is due ("Magmadar:
  Panic - fear! Tremor Totem / Fear Ward in about 5 sec"). Instant abilities are predicted from the script timers (earliest time
  minus lead) and re-anchored when the aura lands on the boss or the raid. Config: `Playerbot.Raid.BossWarnings` (default 1).
  Add a boss by adding rows to the table; tests in tests/game/ClassicBossWarnings.cpp.
