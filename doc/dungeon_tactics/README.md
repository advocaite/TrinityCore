# Dungeon tactics tables

Step 4 of the game plan: what every mob of a dungeon casts, how often, and what the party should do about it, as data the bots read before
a pull. One page per dungeon in this folder, one generated C++ table per dungeon in `src/modules/PlayerbotV2/Bot/Dungeon/Tactics/`.

Batch 1: Deadmines, Ragefire Chasm, Wailing Caverns, Shadowfang Keep, Blackfathom Deeps. Batch 2: Stockade, Scarlet Monastery, Razorfen Kraul, Gnomeregan, Razorfen Downs. Batch 3: Uldaman, Zul'Farrak, Blackrock Depths, and Molten Core trash (map 409 is not a 5-man; the raid brain reads RaidEncounterData_MC, so these rows are reference only until wired).

## Where the data comes from

| Data | Source |
|---|---|
| Which creatures a dungeon has, their level, rank, type, class, mechanic and school immunities | VMaNGOS `creature_template` and `creature` (map spawns) of the 2 October 2017 world dump (`vmangos/core`, git history, `sql/world_full_02_october_2017.7z`) |
| What each creature casts, on what target, at what timer, in which health window, what it summons, at what health it flees | The creature AI Forever's Classic world runs: `sql/custom/world/2026_09_28_15_world_vanilla_smartai.sql` (the VMaNGOS 2021 EventAI and spell lists converted to SmartAI), including the action lists they call |
| Damage and heal of a cast, ground and damage-over-time effects | `classic_spell_value` (VMaNGOS 1.12 spell values, step 1 of the game plan): mean of base points + die, not the client's retail-scaled numbers |
| Bosses whose casts sit in a C++ script | read by hand from `vmangos/core` `src/scripts/...`, `tools/dungeon_tactics/boss_manual.py` (Mr. Smite so far) |
| What a spell id *is* (heal, fear, root, summon, absorb), its school, cast time, dispel type | `tools/dungeon_tactics/spell_facts.py`, written by hand. A `*` after a spell in the docs (flag `SF::Unverified`) means the meaning was recalled and has not been checked against the client data. Spells with no entry show as "spell NNN" and are treated as plain melee. |

The creature types and immunities decide the crowd control column: Polymorph (humanoid, beast), Sap (humanoid), Freezing Trap, Shackle (undead),
Banish (demon, elemental), Hibernate (beast, dragonkin), Scare Beast, each dropped when the mob is immune to the mechanic or is a boss.

## How the bots use it

`Bot/Dungeon/Tactics/DungeonTactics.h` has the lookup (`FindMob`, `TacticsForMap`) and `AppendAdvice`, which turns a map's tables into the lists the
dungeon AI already reads (`DungeonAdvice`): kill order (priority 2 and up, highest first), mandatory interrupts, crowd control targets,
dispel priorities and enemy buffs to purge. `DungeonScriptMgr::GetAdvice` calls it after the hand-written script, so a script's own entries keep
their place at the front. Switch: `Playerbot.Behavior.DungeonTactics` (default 1, Classic realms only).

The tables hold more than those lists use today: per spell the timer window, the victim distance window, the health windows, the mean damage, the
severity and the school bits to hold back (`holds_schools`: Divine Protection is physical, a Quick Flame Ward is fire). Those are for the next
wiring steps (pull-back pathing around casters, healer cooldowns before a known burst, school hints on a ward).

## Reading a table

- *Kill priority* uses the `Raid::Priority` scale: 5 heals or controls the party, 4 summons or a hard-hitting caster, 3 buffs allies or drains,
  2 caster, 1 default. Bosses stay at 1.
- *Severity* 0 trivial, 1 notable, 2 dangerous, 3 must be answered. It is the mean damage against a rough level-appropriate health pool
  (`50 + 30 x level`, 680 at level 21), raised for fear, charm, sleep, stuns, summons, enemy heals and absorbs.
- *Counter*: interrupt (it has a cast time and is worth a kick), dispel (the victim's debuff), purge (the mob's own buff), step out (area), hold damage.
- *Flees*: the mob runs at the given health percent and brings other packs. Finish it with ranged or burst, do not chase it.

## Regenerating

```
python3 -I tools/dungeon_tactics/gen_tactics.py --batch N          # needs the VMaNGOS 2017 dump, see vmdata.py (env VMANGOS_DUMP)
python3 -I tools/dungeon_tactics/gen_tactics.py --batch N --check  # spells that matter and have no entry in spell_facts.py
```

A new `DungeonTacticsData_*.cpp` needs a cmake reconfigure. Tests: `ctest -L ClassicDungeonTactics`.
