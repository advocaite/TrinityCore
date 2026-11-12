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
- A line that starts with the name of a bot in your group, or with `all`, is that bot's order (every bot's for `all`)
  with no prefix needed: `Lentha use altar`, `Baxter, follow`, `all use altar`. Anything that is not an order after the
  name is ignored, so talking about a bot in chat does nothing.
- A `;` in front of a party line (`;tank: attack`) still sends it through the full whisper parser, including address
  prefixes.
- **Address prefixes** (whisper or `;`): `all:`, `squad:`, `here:`, `tank:`, `healer:`, `dps:`, `<class>:` (for
  example `mage:`), `<spec>:`, `<BotName>:` before the order. ike3-style selectors also work with
  `Playerbot.Behavior.Ike3Commands`: `@skull`, `@star`, ... (bots holding that icon or targeting that mob),
  `@tank`, `@healer`, `@dps`, `@ranged`, `@melee`, `@<class>`.
- The whisper command prefix `Playerbot.Interact.CommandPrefix` (default `!`) exists in the config. **(unverified:**
  whether the parser still needs it; plain whispers work without it.)
- Anything not recognised is silently dropped (and logged, see `[owner_order]`).

## 2. Getting bots: add, call, save, dismiss

The `.bot` commands for your own bots work for every account, no GM needed; each one only touches bots owned by your account. `.bot world`, `.bot population`, `.bot bgstress`, `.bot bgsoak` and `.bot itemstats` need GM (level 3). The addon buttons run the same commands.

| Command | What it does |
|---|---|
| `.bot add <class> [tank\|healer\|dps] [race] [male\|female]` | Creates a companion bound to you, levels it to you, teaches spells, sets role/talents/gear, brings it over and groups it. The role is kept across relog and `.bot call`. |
| `.bot lfg [tank\|healer\|dps]` | Fills the missing tank / healer / damage places of your group (1 tank, 1 healer, 3 damage) with level-matched companions, now. The word is your own role. The Group Finder does the same on its own: post a dungeon listing and, if nobody joins within `Playerbot.Lfg.BotFillSeconds` (default 180), the missing roles are filled. |
| `.bot dungeon [list \| <name\|any> [tank\|healer\|dps] \| leave \| cancel]` | Dungeon finder. `.bot dungeon any` (a random 5-player dungeon for your level) or `.bot dungeon <word>` (`rfc`, `vc`/`dm` Deadmines, `smgy`, `smlib`, `dme`, `strat`, `ubrs`...; `.bot dungeon list` shows them): the missing tank / healer / damage places are filled with your companions at your level, and once the group is complete (at most 90 s) you are put inside the dungeon's door and your bots are brought after you. Real players in your group get a summon. `.bot dungeon leave` takes you back to where you queued, bots follow; `cancel` stops a queue still forming. Only the group leader queues. `Playerbot.Lfg.Finder.Enable` turns it off. |
| `.bot call <name\|all>` | Brings back a companion (logs it in, joins your group). `all` = your saved roster plus every bot in your group. |
| `.bot summon <name\|all\|saved>` | Every player, own bots only: `.bot summon all` brings only the bots that are in your group right now (it never logs anyone in), `.bot summon saved` your saved roster plus the grouped bots (logging them in), `.bot summon <name>` one of your companions. Any other name: "You can only summon your own bots". The GM `.summon` (other players) is unchanged and stays GM-only. |
| `.bot save <name\|all>` | Puts a companion (or every bot with you) on your roster. |
| `.bot unsave <name\|all>` | Takes it off the roster; it stays yours and `.bot call <name>` still works. |
| `.bot dismiss <name\|all>` | Removes it from your group and logs it out. Gear is kept. |
| `.bot delete <name>` | Deletes the character for good (the addon asks first). Only bots made for you: one of your own characters summoned as a bot (`.playerbot summon`) is refused with a pointer to `.bot dismiss`. |
| `.bot list` | Your companions, online or not (`saved` marks roster ones). |
| `.bot regear <name\|all>` | Re-rolls the equipment of online companions so they do not all wear the same pieces. Better pieces they wear stay; replaced ones go to the bags. |
| `.bot world` | Status of the world-bot spawner (Playerbot.World.*), console too. |
| `.bot population [off\|low\|medium\|high\|file]` | How many world bots the realm has, GM only, until the next restart (the launcher's Bots setting, `Bots.Size`, decides at every start; the add-on has no World bots tab since 0.6.13). Applies right away (a step down logs the world bots out first). It wins over `Bots.Size` and `Playerbot.World.Enable` until then; `file` hands back to the config files. No word: what it is now. Console too. |

Group size is capped by the map you stand in, see [Instance rules](#8-instance-rules). In a group that is full,
`.bot add` and `.bot call` tell you why instead of silently failing.

## 3. The ForeverBots addon (`/fb`)

Install the `ForeverBots` folder into `Interface\AddOns` (the folder name must stay `ForeverBots`; tick "Load
out of date AddOns"). The addon only sends ordinary chat, so it needs nothing else on the client.

- `/fb` opens or closes the window (it stays open across relog). The window has:
  - **Raid ready** (also `/fb raidready`): calls your saved bots (`.bot summon saved`) when you are alone (already in a group it uses that group and never changes it), waits until the group stops growing,
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
| `defensive` | yes | yes | The start stance (the panel's Defensive): starts nothing, fights what you pull or what attacks the group, joins the group's fight. `Raveni: defensive, defending only until you say attack or pull`. |
| `aggressive` | yes | yes | Also attacks anything hostile within about 10 yards of you: `Raveni: aggressive, attacking anything hostile within 10 yards of you`. |
| `passive` | yes | yes | The panel's Passive: attacks nothing at all, not to defend itself or you, not on the group's fight, not on attack or pull, until `defensive` or `aggressive`. Heals, buffs and other friendly spells go on; its pet goes passive and follows. `Raveni: passive, I attack nothing (I still heal and buff) until you say defensive or aggressive`. |
| `ignore target` / `ignore clear` | yes | yes | Your selected mob goes on (or, again on it, off) the list of mobs none of your bots attack, even when it attacks you or them (a beast you tame); the other mobs are fought as usual, and no area spell goes out while it is within 30 yards of the bot. `ignore clear` empties the list. Answered once as a system line. Memory only, up to 8 mobs, each forgotten after 30 minutes. |
| `place <shape> [tight\|normal\|wide] [here\|target\|cursor] [who...]` / `place follow [who...]` | yes | yes | The ForeverBots Positions tab. The chosen bots stand in a shape and stay there: `close`, `circle`, `arc`, `spread` (far apart, about 10 yards on a normal gap), `line`, `column`, `spear`, `arrow`. `here` (the default) puts the shape in front of you (never on your own spot) and points it the way you face; `target` puts it round or in front of your target, facing you (not a member of your own group); `cursor` waits up to 30 s for you to point at the ground: the add-on's At cursor button (green ground circle, a world marker) or a ping (G key / ping wheel), and centres the shape there, facing away from you. Circle and arc go round the spot. Who: bot names, `all`, `tanks`, `healers`, `melee`, `ranged`, `dps`, `group1`..`group8` or a class; none = all your bots in the group. Tanks take the front, healers the back. After every fight they walk back to their own spot, until `place follow` (or Follow) lets them follow again. Answered once as a system line; the add-on does not wait for a reply. |
| `resume` | yes | yes | Autonomous again; `Resume: autonomous.` |
| `stop` / `disengage` | yes | `disengage` | Stop attacking: `Raveni: stopped`. |
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
| `focus <name\|clear>`, `target <entry>`, `mark <icon>`, `interrupt`, `cc` | yes | no | Finer combat control. |
| `focus heal <name>, <name>` / `focus heal me` / `focus heal off` | yes | no | A healer heals only those group members and itself until `focus heal off` (relog clears it). Bare `focus heal` says who it is on. |
| `aoe on\|off` (bare `aoe` toggles), `aoe_all on\|off` | yes | no | Off: the bot casts no spell that hits more than one enemy (area and cone spells, damaging ground areas such as Blizzard or Consecration, chained spells such as Chain Lightning, Multi-Shot or Cleave, Magma and Fire Nova Totem) until on. `aoe_all` sets every bot of the group. Memory only; `clearprefs` turns it on again. |

### Questions and housekeeping (OwnerCommands)

| Order | Whisper | Group | What the bot answers |
|---|---|---|---|
| `help` / `?` | yes | yes (one bot) | Four lines: the orders, the questions, town and loot, raid words. `commands` prints the whole verb index. |
| `who` | yes | yes | `Raveni: level 60 Warrior (Protection), tank - following you`. |
| `stats` | yes | yes | `Raveni: health 3120/3120 (100%), item level 66, armor 2250, str 210 agi 110 sta 290 int 40, crit 4.1%`. Mana is added for mana users. `status` is the older diagnostic dump. |
| `gear` / `inventory` | yes | yes | `Raveni: 15 pieces worn, avg item level 66, best: Ironfoe, ...; 14 free bag slots; 12g 40s 5c`. `inv` lists bag items; `equipped` lists each worn slot. |
| `money`, `level`, `xp`, `dur`, `skills`, `recipes`, `rep`, `spec` | yes | no | One-line introspection verbs. |
| `change talents <link>` (also `set talents`, `talents <link>`) | yes | no | Learns your own build. Make it in the Wowhead Forever talent calculator (https://www.wowhead.com/forever/talent-calc), copy the page link (or just the code part, such as `--05003201`) and whisper it. The bot drops its talents, learns yours and plays the spec of ours closest to it (a deep Protection build tanks). Below 60 it fills the tree with most points first, row by row, and puts each new level's point down your build: `Talents: learning your build 0/31/20: 21 points now, the rest as I level.` A link for another class, a code from another calculator or a build the game would not allow is refused with the reason. Kept across relog for your companions. `talents link` answers with the bot's talents as a calculator link; `talents default` (or a `setspec`) gives it our builds back. |
| `sell grey` / `sell junk` / `sell your junk` / `s *` | yes | yes | Sells everything a vendor visit sells (greys, trade goods for no profession of the bot's, gear it can't use, outgrown greens when the bags are tight) to the vendor you have selected, else the nearest friendly vendor within 200 yards. In reach: `Raveni: selling 7 thing(s) to Dolanaar Vendor`. Farther away it walks over and sells when it arrives (`at Dolanaar Vendor, selling 7 thing(s)`; after a minute without getting there it says so). Nothing to sell: `nothing to sell, my bags only hold things I keep (0 free slots)`. No vendor: `no vendor near here`. Plain `sell` (selected vendor) and `sell <bag> <slot> [n]` also exist. |
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
- **The group's fight** (your bots, outside raid boss fights): once you or a member near you is in combat, every bot fights,
  with no order needed. Damage dealers take your `attack` / `focus` / skull target, the dungeon's priority add, the tank's target,
  your target, then the nearest mob on a member; tanks first take any mob off a non-tank; healers heal; a bot that cannot reach or
  see its target moves until it can. A `stop`, `flee`, `follow`, `stay` or `pull back` from the last 10 s (or given during the fight)
  still wins, and so does the `passive` stance (it fights nothing). Defensive, Stay, follow, formation and leashes are out-of-combat only: a bot left on an older Stay fights, then walks
  back to its spot. Raid tactics (threat holds, the ranged spots behind the tank) are for 20 and 40-player raids; a UBRS 10-man
  plays as a dungeon. The log names why a bot stood idle in a fight (`[group_idle]`).
- **Marks need an order** (`MarksNeedOrder`, default on): putting an icon on a mob never starts a fight by itself.
  Bots engage after `attack`, `pull`, `pull back`, `pull rti` or `focus`.
- **Boss tactics, kept per boss**: `tactics Lucifron: adds first, no aoe, tank here`. Your plan for a raid boss, in plain words; the
  bots follow it every time that boss is in the fight, and it is kept (per account, across restarts). Words: `adds first` (the damage
  dealers leave the boss to the tanks while one of his adds is up), `boss first` (they stay on the boss, the tanks keep the adds),
  `any order`, `no aoe` / `aoe ok`, `no fear` / `no sheep` / `no banish` ... (that crowd control is not used in that fight; `fear ok`
  takes it back), `tank here` (the tank holding the boss drags him to where you stand; say it inside the raid), `tank anywhere`,
  `clear`. Without a name: the boss you have targeted (`tactics: boss first`). `tactics Lucifron` shows his plan, `tactics` lists
  all of yours. Party / raid chat, a whisper to any of your bots, or `;` lines; one answer. With the bot bridge on, a free sentence
  works too ("on Lucifron kill the adds before him and nobody fear"). Raids the bots know: Molten Core, Onyxia, Blackwing Lair,
  Zul'Gurub, Ahn'Qiraj, Naxxramas. A boss whose fight already has a set tank place (Majordomo) keeps it.
- **Crowd control on and off, per class**: `@warlock no fear`, `@mages no sheep`, then `@warlock yes fear`, `@mage yes sheep` to
  allow it again. Without a class the order is for every bot (`no fear`, `yes fear`). `off cc` / `on cc` (also `cc off` / `cc on`)
  stops or allows all crowd control for every bot, and `@warlock off cc` for one class. Party / raid chat, a whisper to any of your
  bots, or `;` lines; it is your setting, not the bot's, so it also covers bots of that class who join later, and you get one
  answer (`Your warlocks will not use fear now (left out for them: fear).`). Kinds: `fear` (Fear, Howl of Terror, Death Coil,
  Psychic Scream, Intimidating Shout, Scare Beast, Turn Undead), `sheep` / `poly`, `banish`, `shackle`, `sap`, `sleep` (Hibernate,
  Wyvern Sting), `trap` (Freezing Trap), `charm` / `seduce` (Seduction, Enslave Demon, Mind Control), `cc` (all of them). Stuns,
  roots, snares and interrupts are not crowd control here and stay in the rotations. A denied spell is skipped and the bot goes on
  with its next spell; marked crowd control picks another spell or no bot for that mob. Fear is off until you allow it
  (`yes fear`, `@warlock yes fear` or `on cc`): a feared mob runs into the next pack. Memory only: a server restart goes back to
  everything allowed but fear. The panel's CC icon (`crowdcontrol on|off`) stays the switch for marked crowd control alone.
- **The bot's own picks (whisper)**: `prefer blessing might|wisdom|kings|salvation|light|sanctuary`, `prefer aura
  devotion|retribution|concentration|shadow|frost|fire|sanctity`, `prefer earth stoneskin|strength|tremor|earthbind|stoneclaw`,
  `prefer fire searing|nova|magma|flametongue|frost`, `prefer water mana|healing|poison|disease|fire`, `prefer air
  windfury|grace|grounding|nature|windwall|tranquil`, `prefer aspect hawk|monkey|wild|cheetah|pack`, `prefer demon
  imp|voidwalker|succubus|felhunter`; `prefer <slot> auto` returns that slot to the plan, `prefer clear` all of them, a bare
  `prefer` lists them. The pick replaces the group plan for that choice only: a pinned paladin gives its blessing to everyone in
  reach (a Greater Blessing per class with Symbol of Kings; Salvation never on a tank) and the other paladins leave it out of
  their split; a pinned aura counts as taken for the party; a pinned totem stands in for its element (a damage fire totem only
  near a living victim); Cheetah and Pack are held out of combat only (Hawk in a fight); a pinned demon is summoned and never
  sacrificed. Refused for another class (`Prefer: I am no paladin, ...`) or a spell the bot does not know yet. `pet off` dismisses
  a hunter's or warlock's pet and keeps it away until `pet on`; `stealth off` keeps a rogue from opening out of Stealth (and from
  Sap) until `stealth on`. Memory only: a relog or restart clears them.
- **Warlock summon and soulstone**: `ritual <name>` casts Ritual of Summoning on that member of the group; two more members click
  the portal (`use summoning` in party chat sends bots). `soulstone <name>` stones that member (bare `soulstone`: your target, else you).
- **Mage portals**: `portal <city>` opens a portal, `teleport <city>` takes the mage there. The six capitals are found by spell id, so the
  English names (`ironforge`, `if`, `storm`, `thunder bluff`, `tb`...) and the Russian ones work whatever language the server's spell names are in.
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

- **Open world**: you have at most `Playerbot.Behavior.OpenWorldGroupCap` (default 40: no limit beyond the raid; 5 = a normal party) of your own companions out at
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
- **Guild invite by whisper**: any player can whisper a bot "invite me to your guild", "can I join your guild", "ginv",
  "пригласи в гильдию", "можно в гильдию" and gets a normal guild invite (whatever `Guilds.InvitePlayers` says). The
  whispered bot invites when it is the guild master or an officer, else an online officer bot of its guild does. Only
  bot-run guilds; the player must be guildless and of the same faction. A short "yes" / "inv" / "да" works for 3 minutes
  after the bot talked about its guild to that player, and for 30 minutes after its "/w <name> for invite" Trade line.
  When no invite can go out the bot says why. `Bot/GuildWhisperInvite.cpp`, log `[guild_whisper]`.
- **Politeness** (`PoliteWorldBots`): a trade request is cancelled with "sorry, nothing to trade"; a group
  invite is declined with a short whisper unless the bot is free to group; a duel from a stranger is accepted about
  one time in three when the levels are within 5 and nothing is fighting, otherwise declined with a whisper.
  Invites: `Playerbot.Interact.AutoAcceptInvites` (`none`, `friends_or_higher`, `guild_or_higher`, `anyone`).
- **No invites to passers-by** (`InvitePassingPlayers` 0): a world bot never sends a group invite to a real player who
  walks past; it only invites other bots. A player invites the bot himself (or whispers `invite` to it). 1 = the old way (invites players within 30 yards).
- Companion banter (`PartyBanter`): a short lowercase line now and then on ready check, wipe, rez, death, boss
  kill; never in a group over 15 or with `Playerbot.Chatter.Enabled = 0`.
- **Raid leader banter** (`Playerbot.Raid.Banter`: 0 off, 1 mild (default), 2 harsh): in a group with a real player, one bot
  leads the raid's chat (the group leader if that is a bot, else an assistant bot, else the first bot) and reacts to mistakes:
  standing in fire or another bad area, a cast nobody kicked, a broken crowd control (named when the server saw who hit it), an
  extra pull. After a wipe one or two other bots answer (blame, excuses, "my bad"), and the kill gets a "gg". Mild teases, harsh
  is a leader who freaks out at every mishap. Game mistakes only, never about the person, never a slur. Rate limited (one line
  every 20 s, 10 s on harsh; the same mistake once in 45 s; the same raider once in 90 s). In a party of 15 or fewer with
  `PartyBanter` on, the wipe and kill lines are the companions' own. No model needed; with the bot bridge answering chat, the
  program may say it in the bot's own words, and a line not back within 2.5 s is the fixed line instead. Russian clients get
  Russian.
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
| `Playerbot.Behavior.InstanceGroupCap` | 1 (on; 0 lifts the limit of companions to what the map holds) |
| `Playerbot.Behavior.OpenWorldGroupCap` | 40 (no limit beyond the raid; 5 = a normal party) |
| `Playerbot.Pve.Coordinator.Enable` | 1 |
| `Playerbot.Raid.Enable` | 1 |
| `Playerbot.Raid.BossWarnings` | 1 |
| `Playerbot.Raid.Callouts` | 1 |
| `Playerbot.Raid.Banter` | 1 (0 off, 1 mild, 2 harsh) |
| `Playerbot.Behavior.DungeonFlow` / `DungeonAutopilot` / `DungeonLoot` | 1 / 1 / 1 |
| `Playerbot.Behavior.PullRecover` / `WipeReleaseWait` / `ReadyCheckInfo` | 1 / 1 / 1 |
| `Playerbot.TankThreatMultiplier` | 3.0 |
| `Playerbot.Raid.TankThreatMultiplier` | 5.0 |
| `Playerbot.Raid.SureTaunt` | 1 |
| `Playerbot.Scenario.OwnerThreat` | 1 |

**Raid combat behaviour (all default 1)**: `RaidGear`, `AmmoSupply`, `TankRangedWeapon`, `HealClaims`, `CastFacing`,
`GroundSnap`, `ReFace`, `AvoidCircles`, `PackStop`, `RaidSpacing`, `SpreadOut`, `LooseMobHold`, `RaidBuffHold`,
`WarriorStanceDance`, `PurgeDuty`, `RangedSlow`, `SoloFlee`, `DruidHealForm`, `PullDiscipline`. Gear:
`Playerbot.Gear.MaxTier` (1), `Playerbot.Gear.RaidIlvlCap` (0).

**Social and world (default)**: `BotWhisperable` 1, `WorldBotSayReply` 1, `PoliteWorldBots` 1, `InvitePassingPlayers` 0, `PartyBanter` 1,
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

- The ike3 strategy toggles `co`/`nc`/`ss` (+/- strategy lists) are **not implemented**. Trade is: the bot takes
  gifts from its owner or a group mate (see Gifts by trade) and a free world bot buys what you put in. The
  nearest equivalents are `max dps` / `save mana` / `normal`, `aoe`, `hold`/`defensive`/`aggressive`.
- `repair` acts at once only when the vendor is within interaction reach; from up to 60 yards the bot walks over and
  you repeat the order. `sell junk` walks over and sells by itself. The bot stays by the vendor afterwards ("follow" to
  bring it back); the bots' own vendor upkeep (idle vendor visit, bag-full recovery) still runs when they are not in a group.
- `loot all` is held in memory per bot and resets on relog.
- `defensive` / `aggressive` / `passive` are the three owner stances (BotAI::OwnerStance Passive / Aggressive / NoFight); there
  is no separate attack-stance engine.
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

Gifts by trade: open a trade with the bot, put the items in, accept. Gold, gear that is an upgrade and reagents of a recipe the bot knows are gifts; anything else the bot buys at a fair price. The bot accepts, then its auto-equip rules wear what is an upgrade for its role (within a few minutes in the open world, at once in a dungeon, or say `equip upgrades`); the rest stays in its bags.

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
A name that fits no item is tried against the recipe names, so a mage answers `craft water x20` and `craft food x20` with its best
Conjure Water / Conjure Food rank. `water` / `drink` and `food` (also the Russian `вода`, `воду`, `напиток`, `еда`, `еду`, `пища`) find
those ranks by the spell data, so they work whatever language the server's item names are in. Of the items a name fits, the best one you are high enough to use is made. Conjured items cannot go
by mail: the bot asks you to open a trade with it and puts them in the window.
A bare `craft <spell id>` is the older owner command and still casts that recipe once. Idea and parser from Trixx34/ForeverBots (GPL-2.0).
