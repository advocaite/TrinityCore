# WoW Forever Bots: calling, ordering and troubleshooting your bots

This is the player-facing guide to WoW Forever Bots companions (and the bots that live in the world). Everything
here was checked against the source of this branch; lines marked **(unverified)** or **(not implemented)** say
what could not be confirmed. Server operators: the config keys are in `conf/playerbot.conf.dist`.

## Contents

1. [The three ways to give an order](#1-the-three-ways-to-give-an-order)
2. [Getting bots: add, call, save, dismiss](#2-getting-bots-add-call-save-dismiss)
3. [The ForeverBots addon (`/fb`)](#3-the-foreverbots-addon-fb)
4. [Owner orders, with examples](#4-owner-orders-with-examples)
5. [Marks, crowd control and tank assignment](#5-marks-crowd-control-and-tank-assignment)
6. [Formations, spacing and distance](#6-formations-spacing-and-distance)
7. [Raid orders](#7-raid-orders)
8. [Instance rules](#8-instance-rules)
9. [Battlegrounds](#9-battlegrounds)
10. [Quests](#10-quests)
11. [World bots](#11-world-bots)
12. [Config keys](#12-config-keys)
13. [Log tags for troubleshooting](#13-log-tags-for-troubleshooting)
14. [Known gaps](#14-known-gaps)

---

## 1. The three ways to give an order

Only the **owner** of a bot (the account that added or adopted it; for an unowned bot, its group) can order it.
Orders are case-insensitive plain words.

| Channel | How | Who answers |
|---|---|---|
| **Whisper to one bot** | `/w Raveni follow` | That bot only. It always answers with a short confirming whisper (`Raveni: following at 5 yards`). |
| **Party or raid chat** | type the bare word, e.g. `follow` | Every bot of yours in the group acts. Questions (`who`, `help`) are answered by one bot, the lowest-guid one, so a raid does not answer forty times; per-bot questions (`stats`, `gear`) are answered by every bot while you have at most 8 in the group, otherwise by one. |
| **The panel** | `/fb` window and the per-bot panel next to a targeted group member | The buttons send the same words as chat or whispers. |

Rules worth knowing:

- Only words on the group allow-list are picked up from party/raid chat. Ordinary chatter is ignored. Words that
  are not on the list (for example `inv`, `repair_all`) work by whisper only.
- A `;` in front of a party line (`;tank: attack`) sends it through the full whisper parser, including address
  prefixes.
- **Address prefixes** (whisper or `;`): `all:`, `squad:`, `here:`, `tank:`, `healer:`, `dps:`, `<class>:` (for
  example `mage:`), `<spec>:`, `<BotName>:` before the order. ike3-style selectors also work with
  `Playerbot.Behavior.Ike3Commands`: `@skull`, `@star`, ... (bots holding that icon or targeting that mob),
  `@tank`, `@healer`, `@dps`, `@ranged`, `@melee`, `@<class>`.
- The whisper command prefix `Playerbot.Interact.CommandPrefix` (default `!`) exists in the config. **(unverified:**
  whether the parser still needs it; plain whispers work without it.)
- Anything not recognised is silently dropped (and logged, see `[owner_order]`).

## 2. Getting bots: add, call, save, dismiss

`.bot` commands need a GM account (level 3 on the realm). The addon buttons run the same commands.

| Command | What it does |
|---|---|
| `.bot add <class> [tank\|healer\|dps] [race] [male\|female]` | Creates a companion bound to you, levels it to you, teaches spells, sets role/talents/gear, brings it over and groups it. The role is kept across relog and `.bot call`. |
| `.bot call <name\|all>` | Brings back a companion (logs it in, joins your group). `all` = your saved roster plus every bot in your group. |
| `.bot summon <name\|all\|saved>` | Every player, own bots only: `.bot summon all` brings only the bots that are in your group right now (it never logs anyone in), `.bot summon saved` your saved roster plus the grouped bots (logging them in), `.bot summon <name>` one of your companions. Any other name: "You can only summon your own bots". The GM `.summon` (other players) is unchanged and stays GM-only. |
| `.bot save <name\|all>` | Puts a companion (or every bot with you) on your roster. |
| `.bot unsave <name\|all>` | Takes it off the roster; it stays yours and `.bot call <name>` still works. |
| `.bot dismiss <name\|all>` | Removes it from your group and logs it out. Gear is kept. |
| `.bot delete <name>` | Deletes the character for good (the addon asks first). |
| `.bot list` | Your companions, online or not (`saved` marks roster ones). |
| `.bot regear <name\|all>` | Re-rolls the equipment of online companions so they do not all wear the same pieces. Better pieces they wear stay; replaced ones go to the bags. |
| `.bot world` | Status of the world-bot spawner (Playerbot.World.*), console too. |

Group size is capped by the map you stand in, see [Instance rules](#8-instance-rules). In a group that is full,
`.bot add` and `.bot call` tell you why instead of silently failing.

## 3. The ForeverBots addon (`/fb`)

Install the `ForeverBots` folder into `Interface\AddOns` (the folder name must stay `ForeverBots`; tick "Load
out of date AddOns"). The addon only sends ordinary chat, so it needs nothing else on the client.

- `/fb` opens or closes the window (it stays open across relog). The window has:
  - **Raid ready** (also `/fb raidready`): calls your saved bots (`.bot summon saved`), waits until the group stops growing,
    makes it a raid when it holds more than five, sets formation circle and spacing close, sends `resume` and the
    crowd-control switch, sets the Main Tank flag when you target a group member, and starts a ready check about twenty
    seconds later while the bots buff themselves. Not in combat; a new click or combat cancels it.
  - **Add buttons**: one per class and role (`.bot add <class> <role>`).
  - **Orders**: Follow, Stay, Attack (your target), Resume, Pull, Flee, Come, Stop, Release, Pull back, Fall back,
    Set rally. They go out in party/raid chat, so typing the word is identical.
  - **Formation** (Circle, Arrow, Line, No form.) and **Spacing** (Tight, Close, Spread), and the **Distance**
    stepper (1 to 20 yards).
  - **Assign a bot to an icon**: eight raid icons, a Role button (auto, tank, cc, kill), a list of what the server
    holds, Clear all.
  - **Tank markers**: a marker button, Tank target, Clear tanks, and a Main tank entry.
  - **Your bots**: Call saved, Save group, Dismiss all, Unsave all, Delete target, List, Regear all.
- **Per-bot panel**: when your target is a friendly party or raid member, a small panel appears next to the target
  frame (drag it, position is saved): the eight icons (click assigns, right-click clears), Follow, Stay, Attack,
  Pull back, Dist -/+, Dismiss and a status line with the bot's last confirmation. `/fb bot on|off` toggles it.
  The client cannot tell a bot from a human, so do not use the panel on real players.
- Slash commands that work without the window: `/fb add <class> [role]`, `/fb raidready`, `/fb call|dismiss|save|unsave|regear
  [name|all]`, `/fb delete <name>`, `/fb list`, `/fb fallback`, `/fb pullback`, `/fb rally`, `/fb formation
  circle|arrow|line|none`, `/fb spacing tight|close|spread`, `/fb distance <1-20>`, `/fb maintank [name]`, `/fb tank
  <name> <marker>|clear`, `/fb assign <name> <marker> [role]`, `/fb assign list`.
- Confirmation whispers from bots are hidden from your chat frame by the addon (they feed the status line).

There is also an older `PlayerbotControl` addon (roster, stats, alts); this guide covers `ForeverBots`.

## 4. Owner orders, with examples

"Whisper" = `/w <bot> <order>`. "Group" = also accepted as a bare line in party/raid chat. Replies are whispers
from the bot.

### Movement and stance

| Order | Whisper | Group | Example and answer |
|---|---|---|---|
| `follow` | yes | yes | `follow` -> `Raveni: following at 5 yards`. Follow holds: while it stands, bots only defend and do not advance or pull until you say attack, pull or resume. `follow <name>`, `follow tank`, `follow healer`, `follow leader`, `follow stop` also work by whisper. |
| `stay` / `halt` | yes | yes (`stay`) | `stay` -> `Raveni: staying here`. |
| `hold` | yes | yes | Like stay but may fight what attacks it. `Hold: holding position.` |
| `defensive` | yes | yes | Same as hold: `Raveni: defensive, holding here and fighting only what attacks`. |
| `aggressive` | yes | yes | Back to autonomous: `Raveni: aggressive, engaging enemies on my own`. |
| `resume` | yes | yes | Autonomous again; `Resume: autonomous.` |
| `passive` / `stop` / `disengage` | yes | `passive`, `disengage` | Stop attacking: `Raveni: stopped`. |
| `come` | yes | yes | Teleports the bot to you and follows. `Raveni: coming to you`. |
| `summon` | yes | yes | Like `come`, with the summoning refusals: not while the bot is dead or in combat, not while you are in combat, not into a battleground, not into or out of an instance you are not both in. Answer on refusal: `Raveni: can't be summoned, I am in combat`. |
| `flee` / `retreat` | yes | `flee` | Break off and run to you: `Raveni: fleeing to you`. |
| `goto <x> <y>`, `goto <map> <x> <y>` | yes | no | Walk to a point (or travel cross-map by flight/portal/ship). |
| `unfollow` | yes | no | Stop following. |
| `invite` | yes | no | The bot invites you to its group: `Raveni: invite sent`. |
| `leave` | yes | no | The bot leaves the group. |
| `reset` | yes | no | Clears movement, attack and cast: `Reset: cleared movement / attack / cast.` |
| `release` | yes | yes | A dead bot releases and runs to its corpse (or back into the instance). Living bots ignore it. `release_all` by whisper. |
| `mount`, `dismount`, `sit`, `stand`, `hearth`, `bind` | yes | no | Single actions. `*_all` variants (`mount_all`, `hearth_all`, ...) act on every bot in the group. |

### Combat

| Order | Whisper | Group | Notes |
|---|---|---|---|
| `attack` / `kill` / `engage` | yes | `attack` | Attack your selected target: `Raveni: attacking Defias Thug`. In a raid the pull gate may hold DPS until the main tank opens (`not attacking yet (raid pull gate: the main tank opens)`). |
| `assist` | yes | yes | Group `assist` is turned into `attack`. |
| `pull [<sec>]` | yes | `pull` | Your tank bots open on your target, the rest join once it is engaged. With a raid Main Tank flag only that tank pulls. |
| `pull back` / `pullback` / `pull and back` | yes | yes | The main tank (or the assigned/closest tank) opens from range, runs to the rally point and tanks it there. Others hold. Gives up after 30 s. |
| `pull rti` | yes | yes | Opens on the mob carrying the bot's own rti symbol (Ike3Commands). |
| `max dps` / `save mana` / `normal` | yes | no | Burn mode (cooldowns on every fight), healers keep to efficient heals, clear both. Ike3Commands. |
| `focus <name\|clear>`, `target <entry>`, `mark <icon>`, `interrupt`, `cc`, `aoe [on\|off]` | yes | no | Finer combat control. |

### Questions and housekeeping (OwnerCommands)

| Order | Whisper | Group | What the bot answers |
|---|---|---|---|
| `help` / `?` | yes | yes (one bot) | Four lines: the orders, the questions, town and loot, raid words. `commands` prints the whole verb index. |
| `who` | yes | yes | `Raveni: level 60 Warrior (Protection), tank - following you`. |
| `stats` | yes | yes | `Raveni: health 3120/3120 (100%), item level 66, armor 2250, str 210 agi 110 sta 290 int 40, crit 4.1%`. Mana is added for mana users. `status` is the older diagnostic dump. |
| `gear` / `inventory` | yes | yes | `Raveni: 15 pieces worn, avg item level 66, best: Ironfoe, ...; 14 free bag slots; 12g 40s 5c`. `inv` lists bag items; `equipped` lists each worn slot. |
| `money`, `level`, `xp`, `dur`, `skills`, `recipes`, `rep`, `spec` | yes | no | One-line introspection verbs. |
| `sell grey` / `sell junk` / `s *` | yes | yes | Sells grey items to the vendor you have selected, else the nearest vendor the bot sees within 60 yards. In reach: `Raveni: selling 7 grey stack(s) to Dolanaar Vendor`. Farther away it walks over and asks you to say it again. No vendor: `no vendor within 60 yards`. Plain `sell` (selected vendor) and `sell <bag> <slot> [n]` also exist. |
| `repair` | yes | yes | Same pattern with a repair vendor: `Raveni: repairing at ...`. |
| `loot` | yes | yes | Need before greed with the usual etiquette (human gets first claim). `Raveni: need before greed, the usual rules`. |
| `loot all` | yes | yes | The bot rolls for itself on every drop and takes every corpse item, greys included. `Raveni: looting everything, greys included, and rolling for myself`. Memory only (resets on relog). |
| `use <object name or id>` | yes | yes | Uses a dungeon object near the bot: an altar, brazier, lever or orb (`use altar`, `use brazier`). The bot walks to the nearest match within 30 yards, uses it once and then stays there until you say `follow`. For an object that needs several people, such as the Blackrock Altar in Upper Blackrock Spire (three), click it yourself and send it to two bots by name: `;Raveni:use altar`, `;Doran:use altar`. Nothing matching: `no 'altar' within 30 yards of me`. |

### Gear and economy by whisper (selected NPC needed)

`buy <entry> [n]`, `buyfood`, `buypot`, `buybandage`, `buyall`, `equip <entry>`, `bank`, `auction`,
`mail`, `train`, `talents [apply]`, `upgrades [apply]`, `fly <node>` and the `*_all` broadcasts such as
`repair_all`, `sell_all`, `buyfood_all`. They need a selected vendor/auctioneer/flight master where the bot has
to interact. The full index is the `commands` whisper.

## 5. Marks, crowd control and tank assignment

All of these are group-allowed (party/raid chat) and whisperable.

- **Assign a bot to an icon**: `assign <name|target> <marker> [tank|cc|kill]`, `assign clear [marker|name]`,
  `assign list`. Markers: `star circle diamond triangle moon square cross skull`. Mark the mobs yourself with the
  raid target menu; the bot follows its icon. Role auto: a tank bot tanks the mob carrying the icon, a crowd-control
  class (mage, warlock, priest, hunter, rogue, druid, paladin) crowd-controls it until a skull goes on it, anything
  else kills it first. Whispered with no name (`assign star tank`) the whispered bot is the subject. Assignments are
  memory only. Needs `TankMarkers` (tank, kill) and `MarkCc` (cc).
- **Tank markers**: `tank <name> <marker>`, `tank <name> clear`, `clear tanks`. A tank with a marker holds
  whichever mob carries it. Whisper `tank star` to make that bot take Star (`Raveni: tank on Star`).
  Without any, the raid plan splits mobs automatically: main tank marks skull, each off tank the next markers.
- **Kill and CC lists (ike3 rti)**: `rti <symbols>` sets the symbols a bot kills first, `rti cc <symbols>` the
  ones it crowd-controls (skull never), `rti none`, `rti ?` reports, `rti raid` prints the roster and marks.
- **Main tank**: `set main tank <name>` or `main tank <name>` (no name = your target) sets the raid frame's Main Tank
  flag. Raid groups only. With a main tank set, only that tank pulls.
- **Marks need an order** (`MarksNeedOrder`, default on): putting an icon on a mob never starts a fight by itself.
  Bots engage after `attack`, `pull`, `pull back`, `pull rti` or `focus`.
- **Pull back vs fall back**: `pull back` is one tank pulling a mob home to the rally point. `fall back` (also
  `fallback`, `back`) is the whole raid dropping its target and running to the rally point to re-form, see
  [Raid orders](#7-raid-orders).

## 6. Formations, spacing and distance

| Order | Meaning |
|---|---|
| `formation circle\|arrow\|wedge\|line\|column\|none` | Out-of-combat slots round you (tanks in front, healers at the back). `none` is the plain follow. A bot also reports its slot with plain `formation`. |
| `spacing tight\|close\|spread` (also `tight`, `close`, `spread out`) | About 2.5 / 4 (default) / 8 yards. Also scales raid ranged spacing. |
| `distance <1-20>` / `follow distance <n>` | How far bots follow you; sets the spacing yards too. The addon re-sends it on login. Per bot by whisper. |
| `range <yards>` | Picks the nearest spacing preset. Ike3Commands. |
| `slot <n>`, `regroup` | Pin a formation slot; re-snap to the slot. |

In combat and in raid mechanics the raid plan's positions win over the formation.

## 7. Raid orders

| Order | Effect |
|---|---|
| `pull` | The main tank (else every tank bot) pulls your target; the rest follow once it is engaged. |
| `pull back` | See section 5. Exact phrase only, so it never reaches `pull`. |
| `fall back` / `fallback` / `back` | Every bot drops its target and runs to the rally point; tanks form in front, melee just behind, ranged and healers on an arc. They fight only what follows them there and stay until `attack`, `pull` or `resume` (`follow` returns to following you). |
| `rally here` / `rally` | Sets the rally point to where you stand. Without one, fall back uses your position. |
| `release` | After a wipe: dead bots release and run back (into the instance through its portal when they died inside). Bots also release on their own when you do, or after a full wipe. With `WipeReleaseWait` they wait 10-40 s for a rez when a healer lives. |
| `readycheck` / `rc` (whisper) | A bot starts a ready check. Bots answer a ready check on their own; with `ReadyCheckInfo` a low-health/mana or broken-gear bot answers not ready and says why. `ready_all` forces every bot's ack. |

A group chat order reaches every bot of yours; to address only some, whisper them or use a prefix such as
`tank:` or `@skull`.

## 8. Instance rules

- **Open world**: you have at most `Playerbot.Behavior.OpenWorldGroupCap` (default 5) of your own companions out at
  once (`.bot add`, `.bot call`, saved roster, buttons). For more, find world bots and invite them: invited world
  bots are not counted. Companions above the cap (called inside an instance) are dismissed 10 s after you walk out;
  the ones grouped with you stay. Needs `InstanceGroupCap = 1`.
- **Inside an instance**: the cap is what the map holds (5 dungeon; 10, 20 or 40 raid, from the map data; fallback
  5 / 40), for your companions and for the group. `.bot add`, `.bot call` and the buttons stop at it with `<map> is full: n/cap`. Walking into a smaller
  instance with a bigger group warns you once.
- **Battlegrounds and arenas** are not capped by this rule.
- **Solo raid entry**: stock Trinity requires a raid group to enter a raid. Set `Instance.IgnoreRaid = 1` in
  `worldserver.conf` to let a player enter without one. A party (non-raid) group of companions in a raid map
  follows you and defends and does not run the raid autopilot (`OwnerFollowHold`).
- `follow` suspends the dungeon autopilot until you say `attack`, `pull` or `resume`.

## 9. Battleground behaviour

- Bots do **not** queue or fight bot-versus-bot on their own by default (`Playerbot.Bg.AutoSeed.Matches = 0`,
  `Playerbot.Bg.AutoJoin = 0`). When a real player queues, the queue filler supplies both sides, tops up while the
  player is in, and returns the bots to the pool afterwards. It needs `Playerbot.Population.TotalTarget > 0` or
  `Playerbot.World.Enable = 1`.
- Humans have priority: with `Bg.MakeRoomForPlayers` the lowest-scoring bot of a full team leaves to make room
  (never a flag carrier or a bot in combat) (`[bg_queue] MakeRoom`).
- In the match the BG coordinator (`Playerbot.Bg.Coordinator.Enable`) assigns objectives (caps, defence, escort)
  and the Warsong, Arathi and Alterac scripts drive play.
- Owner verbs by whisper: `bg <type>` / `unbg` (queue and leave with a selected battlemaster), `bg_all`,
  `unbg_all`, `bgrun [pause\|stop]`, `pvp`. `summon` is refused into a battleground you stand in.

## 10. Quests

The quest words call `QuestOrders` (`Bot/Quest/QuestOrders.h`). Each answers by whisper; group lines are accepted
for all of them.

| Order | Whisper | Group | What it does |
|---|---|---|---|
| `quests` / `quest list` / `ql` | yes | `quests`, `quest list` | Lists the bot's quest log. |
| `accept` / `accept all` | yes | `accept`, `accept all` | Accepts the offered quests of the quest giver in reach. |
| `drop <quest>` | yes | `drop quest <name>` | Abandons the named quest. Group form needs the word `quest` so ordinary chat ("drop it") never abandons anything. |
| `turn in` | yes | `turn in` | Hands in finished quests. |
| `share` | yes | `share` | Shares the bot's quests with the party. (`qshare <quest_id>` shares one.) |

**Status: these verbs are wired in the parser against `QuestOrders::{List,AcceptAll,Drop,TurnIn,Share}`.** They are
compiled in only when `Bot/Quest/QuestOrders.h` exists in the tree; without it `accept`, `drop`, `turn in` and
`share` answer `quest orders are not available in this build`, and `quests` falls back to the older
`Q<id> L<level>` list. Older helper verbs that always exist: `quest`, `wq`, `abandon <id>`, `abandonall`, `qshare`.

## 11. World bots

World bots are ambient characters with no owner (`Playerbot.World.Enable = 1` spawns them around real players;
`Playerbot.Population.TotalTarget > 0` fills the world). See `WORLD_BOTS.md` for the spawner.

- **Say and yell**: a world bot sometimes answers a real player's greeting, short question or its own name within
  25 yards (60 s per bot, 8 s global, 1-4 s delay). `WorldBotSayReply`, log `[say_reply]`. A bot waves back at
  `hi`/`hello` within 12 yards (`SocialEmotes`).
- **Whispers**: bots answer a whisper from any player (`BotWhisperable`). Only the owner's orders are obeyed.
- **Politeness** (`PoliteWorldBots`): a trade request is cancelled with "sorry, nothing to trade"; a group
  invite is declined with a short whisper unless the bot is free to group; a duel from a stranger is accepted about
  one time in three when the levels are within 5 and nothing is fighting, otherwise declined with a whisper.
  Invites: `Playerbot.Interact.AutoAcceptInvites` (`none`, `friends_or_higher`, `guild_or_higher`, `anyone`).
- Companion banter (`PartyBanter`): a short lowercase line now and then on ready check, wipe, rez, death, boss
  kill; never in a group over 15 or with `Playerbot.Chatter.Enabled = 0`.
- Hostile-faction world bots can be placed near you (`Playerbot.World.FriendlyPct` and the per-zone `*FriendlyPct` keys, all 100 = none by default); lower them to
  get them (they fight on a PvP realm).

## 12. Config keys

Defaults are those of `conf/playerbot.conf.dist`. Behaviour switches are read when the config loads; `1` is on.

**Owner command surface**

| Key | Default | Effect |
|---|---|---|
| `Playerbot.Behavior.OwnerCommands` | 1 | The plain-language words of section 4 (quests, accept, drop, turn in, share, loot, help, who, stats, gear, sell grey, repair, summon, invite, aggressive, defensive) |
| `Playerbot.Behavior.Ike3Commands` | 1 | `@selector` addressing, `pull rti`, `max dps`/`save mana`/`normal`, `rti raid`, `range` |
| `Playerbot.Behavior.OwnerFollowHold` | 1 | `follow` suspends the dungeon autopilot |
| `Playerbot.Behavior.Fallback` | 1 | `fall back`, `rally here` |
| `Playerbot.Behavior.PullBack` | 1 | `pull back` |
| `Playerbot.Behavior.Formation` | 1 | formation and spacing orders |
| `Playerbot.Behavior.DistanceOrder` | 1 | `distance <n>` |
| `Playerbot.Behavior.OwnerMainTank` | 1 | `set main tank` |
| `Playerbot.Behavior.TankMarkers` | 1 | `tank <name> <marker>`, `assign`, auto marking |
| `Playerbot.Behavior.MarkCc` | 1 | icon-driven crowd control, `rti cc` |
| `Playerbot.Behavior.MarksNeedOrder` | 1 | marks only plan; an order starts the fight |
| `Playerbot.Behavior.LootPolicy` | 1 | human-first loot etiquette (`loot all` bypasses it per bot) |

**Group size and instances**

| Key | Default |
|---|---|
| `Playerbot.Behavior.InstanceGroupCap` | 0 (off; 1 limits the companions to what the map holds) |
| `Playerbot.Behavior.OpenWorldGroupCap` | 5 |
| `Playerbot.Pve.Coordinator.Enable` | 1 |
| `Playerbot.Raid.Enable` | 1 |
| `Playerbot.Raid.BossWarnings` | 1 |
| `Playerbot.Behavior.DungeonFlow` / `DungeonAutopilot` / `DungeonLoot` | 1 / 1 / 1 |
| `Playerbot.Behavior.PullRecover` / `WipeReleaseWait` / `ReadyCheckInfo` | 1 / 1 / 1 |
| `Playerbot.TankThreatMultiplier` | 3.0 |

**Raid combat behaviour (all default 1)**: `RaidGear`, `AmmoSupply`, `TankRangedWeapon`, `HealClaims`, `CastFacing`,
`GroundSnap`, `ReFace`, `AvoidCircles`, `PackStop`, `RaidSpacing`, `SpreadOut`, `LooseMobHold`, `RaidBuffHold`,
`WarriorStanceDance`, `PurgeDuty`, `RangedSlow`, `SoloFlee`, `DruidHealForm`, `PullDiscipline`. Gear:
`Playerbot.Gear.MaxTier` (1), `Playerbot.Gear.RaidIlvlCap` (0).

**Social and world (default)**: `BotWhisperable` 1, `WorldBotSayReply` 1, `PoliteWorldBots` 1, `PartyBanter` 1,
`SocialEmotes` 1, `Fidget` 1, `Playerbot.Chatter.Enabled` 1, `Playerbot.Interact.AutoAcceptInvites` friends_or_higher,
`Playerbot.Interact.AutoLeaderHandover` 1, `Playerbot.World.Enable` 0 (`MaxBots` 100, `BotsPerPlayer` 25,
`ActivityRadius` 300, `FriendlyPct` 100), `Playerbot.Population.TotalTarget` 0.

**Battlegrounds**: `Playerbot.Bg.AutoSeed.Matches` 0, `Playerbot.Bg.AutoJoin` 0, `Playerbot.Bg.MakeRoomForPlayers` 1,
`Playerbot.Bg.Coordinator.Enable` 1.

**Realm**: `Instance.IgnoreRaid` (worldserver.conf, default 0) for solo raid entry.

## 13. Log tags for troubleshooting

The module logs nothing unless `Logger.playerbot.v2=3` is set. Grep the server log for the tag.

| Tag | What it tells you |
|---|---|
| `[owner_order]` | Every order a bot receives: source (whisper, party), raw text, resulting order state; unrecognised or refused (not the owner) lines are throttled per bot. Start here when a bot "ignores" you. |
| `[pull_back]` | Which tank was chosen for a pull back and why; every other bot holds. |
| `[fallback]` | A bot's rally slot and rank when it falls back, and its arrival. |
| `[assign]` | Icon assignments: the raw order, the stored result. |
| `[formation]` | Slot, owner position and distance, action taken (rate limited). |
| `[mark_gate]` | A marked mob held back awaiting your order (`MarksNeedOrder`). |
| `[mark_cc]` | Crowd-control assigned, cast, broken or released. |
| `[spread]` | Melee spreading round a victim (every 5 s, one bot). |
| `[role]` | The role a companion has and where it came from (declared, spec, fallback). |
| `[gear]` | Gear pass result: average item level, upgraded slots. |
| `[quest]` | A quest order the parser handed to `QuestOrders` (`[owner_order] [quest] ...`). |
| `[InstanceCap]` | Group larger than the map holds. |
| `[say_reply]` | A world bot's answer to a player's /say. |
| `[world]` | Spawner status line every 60 s. |
| `[death-recovery]`, `[raid_pull]`, `[raid_tank]`, `[solo_flee]`, `[bg_queue]`, `[cast_speed]`, `[GroupRelogin]` | Corpse runs, raid pulls and tanking, fleeing, battleground queues, cast pacing, bots re-joining after your login. |

`/whyidle` (whisper `whyidle`) explains the idle rule a bot chose; `diag` and `state` dump a bot's state.

## 14. Known gaps

- `trade`, and the ike3 strategy toggles `co`/`nc`/`ss` (+/- strategy lists), are **not implemented**. The
  nearest equivalents are `max dps` / `save mana` / `normal`, `aoe`, `hold`/`defensive`/`aggressive`.
- `sell grey` and `repair` act at once only when the vendor is within interaction reach; from up to 60 yards the
  bot walks over and you repeat the order. There is no standing "go and sell, then come back" order; the bots'
  own vendor upkeep (idle vendor visit, bag-full recovery) still runs when they are not in a group.
- `loot all` is held in memory per bot and resets on relog.
- `aggressive` / `defensive` / `passive` map onto the existing manual modes (resume, hold, stop); there is no
  separate attack-stance engine.
- Quest words depend on `Bot/Quest/QuestOrders.h` (section 10).
- Right-click unit-menu entry of the addon and the whisper `!` prefix are **(unverified)**.

## Auction house bot (GM)

GM commands (GM level 3) for the auction house bot (`AuctionHouseBot.*` in the world config):

| Command | What it does |
|---|---|
| `.ahbot status [all]` | Item counts of the AH bot per auction house and quality. |
| `.ahbot stats` | What the buyer spent today and its limits. |
| `.ahbot reload` | Re-reads the AH config. |
| `.ahbot refresh` | Expires the bot's unbid listings and restocks. |
| `.ahbot add [n]` | Adds `n` extra items per quality target (default 500) until the next reload. |
| `.ahbot rebuild [all]` | Stock command. `all` also expires bot listings that have bids. |

The buyer buys players' auctions up to 1.5x the fair price, after a 30-600 s delay, with a 5000 gold daily budget and 60 buys per hour (config keys `AuctionHouseBot.Buyer.*`). Put items up for sale as a player and the bots buy them in their own time.

## Gear

Level-60 (level-cap) companions are geared for raiding by role (`Playerbot.Behavior.RaidGear`): rare and epic pieces of required level 50+ picked by role stat weights, never PvP items, varied per bot among the best few. `.bot regear` (and the periodic gear pass) fill empty slots and swap a worn piece only when the new one scores at least 10% higher.

| Key | Default | What it does |
|---|---|---|
| `Playerbot.Gear.MaxTier` | 1 | Best raid tier the gear may come from, as an approximate item level ceiling: 0 = pre-raid / dungeon (63), 1 = Molten Core / Onyxia (76), 2 = Blackwing Lair (83), 2.5 = AQ40 (88), 3 = Naxxramas (92). Not read from item data. |
| `Playerbot.World.Gear.MaxTier` | 1 | The same scale for world / ambient / BG-fill bots (bots nobody owns); a world bot above it is re-geared down. |
| `Playerbot.World.Gear.EpicPercent` | 15 | Share of a world bot's slots that may hold an epic (0-100); the rest is blue or lower. |
| `Playerbot.Gear.RaidIlvlCap` | 0 | Explicit item level ceiling; above 0 it overrides `MaxTier`. |
| `Playerbot.Behavior.AcceptOwnerTrade` | 1 | A companion accepts a trade its owner opens, once the owner has accepted (gear and money gifts). The bot never offers anything, so nothing can be taken back through the window. Anyone else's trade is declined. Mail is not read by bots, so gifts go by trade. |

Gifts by trade: open a trade with the bot, put the items in, accept. The bot accepts, then its auto-equip rules wear what is an upgrade for its role (within a few minutes in the open world, at once in a dungeon, or say `equip upgrades`); the rest stays in its bags.

| Say or whisper | What the bot does |
|---|---|
| `equip upgrades` / `equip best` / `use your best gear` | Wears every bag item that is an upgrade (raid role score, +10%, at the level cap; the older stat rule below it) and whispers `Raveni: equipped [item] (+ilvl 8), [item] (+ilvl 3)` or `Raveni: nothing better in my bags`. |
| `equip <item link or name>` | Wears that bag item if it can use it, whatever its score. PvP items are refused (`I don't wear those`). `equip <item id>` still works. |

In party or raid chat every addressed bot answers (`all:`, `tank:`, `<Name>:` prefixes select which). Not done: `unequip`.

## Battleground queue (GM)

`.playerbot bgjoin wsg|ab|av|<id>` queues you, plus your group's in-bracket bots, for a battleground without the battlemaster window (`wsg` Warsong Gulch, `ab` Arathi Basin, `av` Alterac Valley, or a battleground type id). The battlemaster's Join button now also works on client 70205. A real player who queues joins a running battleground (the weakest bot leaves to make room) instead of opening a new one.

`.playerbot bgenter` accepts the pending battleground invite (use it if the Enter button does nothing). `.playerbot bgleave` leaves every battleground queue you are in.

## Craft orders (owner and group)

Whisper a bot of your group `craft <item link or name> [x5]` (or `craft list`). The bot makes it with the recipes and reagents it carries
(one craft at a time, up to 20 items) and mails the result to you; missing reagents are whispered back, trade them to the bot and ask again.
A bare `craft <spell id>` is the older owner command and still casts that recipe once. Idea and parser from Trixx34/ForeverBots (GPL-2.0).
