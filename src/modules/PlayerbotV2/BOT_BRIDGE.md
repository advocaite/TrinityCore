# Bot bridge: plug your own program into the bots

The bot bridge lets a program on the same PC as the server see what the bots run into and talk for them, without changing the
server. Some uses: your own chat model answering players, a tool that collects bot bugs, or a helper that looks up quests and
spells. It is **off by default**. With it off, nothing runs and the bots work exactly as they always have.

Each realm has its own bridge: it only reads and writes files in the server folder. Nothing is sent over the internet, and
every player who turns it on picks their own program.

## Turning it on

In `playerbot.conf` (or your own override file):

```
Playerbot.Bridge.Enable = 1
```

`.reload config` works too: the bridge starts or stops on the next world update. The files go to `server\BotBridge\` (setting
`Playerbot.Bridge.Folder`). All settings and what they do are in `conf/playerbot.conf.dist` under "Bot bridge".

## Files

| File | Written by | What |
|---|---|---|
| `events-YYYYMMDD-HH00.jsonl` | server | one JSON object per line, a new file every hour (UTC), or past `RotateMB` as `...-HH00-1.jsonl`. Deleted after `KeepDays`. |
| `replies.jsonl` | your program | one JSON object per line. The server only reads lines added after it started. You may empty the file whenever you like. Once the server has read it to the end and it is over 1 MB, the server renames it to `replies.old.jsonl` (replacing the older one); your next write makes a new file. Open, append, close for each write (as the example does), so the rename never meets an open file. |
| `status.json` | server | every 10 seconds: time, uptime, bot count, last event id, whether your program counts as running, `skipped_repeats` (see Limits). |
| `running.flag` | server | there while the server runs, removed on a clean shutdown. |

The server only reads complete lines (ending in a newline), so your program can write a line in parts. It reads
`replies.jsonl` every `ReadMs` (200 ms), so an answer is picked up within a fifth of a second.

## Events

Every bot event has: `id`, `t` (UTC time), `type`, `bot`, `guid`, `class`, `race`, `level`, `zone`, `area` (when it differs),
`map`, `pos` `[x, y, z]`, and `quest` `{id, name}` (the quest it was asked about, or its first unfinished quest).

| type | When | Extra fields |
|---|---|---|
| `chat_in` | a player whispers a bot, talks in its party or raid, or says something to it in /say (the same lines `Playerbot.ChatModel` answers, with its switches and limits) | `channel`, `from`, `from_is_bot`, `locale` (his game client's language, `ruRU`, `enUS`...: answer a Russian client in Russian when his line shows no language of its own), `text`, `prompt` (a description of the bot and how to answer, the one the built-in chat model uses), `weather` (the zone's: clear, rain, snow, ...), `time_of_day` (morning, afternoon, evening, night), `swearing` (1 when the realm allows everyday bad language, `Playerbot.Bridge.Swearing`), `social` (from a player: the bot's guild and his, the guild charter he carries with its signatures and whether this bot signed it or can, and what the bot would do with his group invite, in words) |
| `moment` | a bot grouped with a real player would say a line on its own in party (it looted something, gained a level, the group wiped or killed a boss, came to a new zone, a ready check, it died or was brought back, it joined the group). Only while a program answers chat and `Moments` is on; the bots' usual chance and cooldowns (`Playerbot.Chatter.*`, `PartyBanter`) decide when, at most `MomentsPerMinute` (6) for the server. Answer it like a `chat_in` (`reply_to` = its `id`; it goes to party; an empty `say` means the bot says nothing). Without a program, or past the limit, the bot says its fixed line as before | `what` (`loot`, `quest`, `kill`, `player_kill`, `level_up`, `greet`, `ready_check`, `wipe`, `rezzed`, `died`, `boss_kill`, `new_zone`, and `need` / `offer`: an owned bot near its owner whose bags are full or gear nearly broken says so, and one that carries an upgrade or a quest item for him offers it, at most one per owner every 5 minutes), `detail` (what happened, in plain words), `example` (the fixed line the bot would have said, an idea; missing for `new_zone`), `to` (the player it is said to: the bot's owner when he is in the group), `to_is_owner`, `can` (with actions on, checked for `to`), `weather`, `time_of_day`, `swearing` |
| `talk` | world bots chat with each other in /say (the gatherings outside the city gates and the street chats inside the capitals, `Playerbot.World.Gathering.*`): the bot's next line. Only while a program answers chat and `Talk` is on, at most `Gathering.ChatModelPerMinute` (4) a minute, and not while two players' lines wait for an answer. Answer it like a `chat_in` (`reply_to` = its `id`, `say` the line); the next bot's `talk` carries it. No answer within 25 s ends that talk | `channel` (say), `scene` (who the bot is, where, with whom, what about), `talk` (the lines so far, `Name: line` each; empty: start the talk), `weather`, `time_of_day`, `swearing`. With `channel` `general` or `trade` it is a world bot's post there (WTS, WTB, LFM, a question; at most `MomentsPerMinute` a minute, the bots' usual suggestion cooldowns decide when) or another bot's answer to one (two posts in five, at most 2 a minute): answer with that `channel`. An item's or quest's name in a post becomes its link; a post that leaves the name out is the bot's fixed line instead. A player who then whispers the bot gets `your_post` in his `chat_in` (its post of the last 15 minutes). A `talk` whose scene says a player walks past is a bot that just waved at him: one short /say line to him, or an empty `say` |
| `stuck` | a bot tried to reach a place for over 30 s without moving and dropped the goal | `detail` |
| `quest_fail` | a bot dropped a quest that it can never finish or turn in | `detail` |
| `spell_fail` | the server refused the same spell `SpellFailCount` times in a minute | `spell_id`, `spell`, `reason` (a server cast-result number; 1000 and up are the bot's own checks) |
| `death_loop` | a bot died `DeathLoopCount` times in 10 minutes in the same place | `killer` |
| `start` | server start | `unclean_stop` (1 = the last run did not shut down cleanly), `last_crash` (the newest file in `Crashes`), `last_crash_age_s` |
| `stop` | clean shutdown, or the bridge turned off | `detail` |
| `dropped` | events left out because more than `MaxEventsPerSec` came in one second | `count` |
| `action_result` | after every action in a reply (only with actions on, see below) | `act`, `ok` (1 or 0), `why` (when refused), `detail` (what was done), `reply_to` |

Example:

```
{"id":1794133205001,"t":"2026-11-08T10:20:05Z","type":"stuck","bot":"Brannor","guid":1234,"class":"warrior","race":"dwarf","level":14,"zone":"Loch Modan","area":"Thelsamar","map":0,"pos":[-5340.1,-2950.3,323.9],"quest":{"id":1920,"name":"Blue Recluse"},"detail":"tried to go to (-5390,-2990,330) for over 30 s without moving; goal dropped (combat=false rooted=false path_fail_recent=true)"}
```

Limits: one event per bot and type every `EventCooldownSec` (300), at most `MaxEventsPerSec` (30) for the whole server. The same
problem coming back inside the 300 s is not lost and not "dropped": the next event of that bot and type carries `repeats`, how many
times it came back in between (and `status.json` counts them all in `skipped_repeats`).
Files are written on their own thread. If the disk can't keep up, the oldest lines are dropped, so the game never waits.

## Replies

```
{"reply_to":1794133205001,"say":"Sure, coming."}
{"reply_to":1794133205001,"say":"Sure, coming.","channel":"party"}
{"bot":"Brannor","say":"Anyone for Deadmines?","channel":"say"}
{"bot":"Brannor","say":"hey","channel":"whisper","to":"Karrz"}
{"ping":1}
```

- `reply_to` answers one `chat_in`. The answer goes on the same channel (a whisper goes back to the sender) unless `channel` says otherwise.
  There's one answer per event, and an answer older than `ReplyMaxAgeSec` (60) isn't said.
- Without `reply_to`, the line is one the bot says on its own. `bot` must be a bot's name (never a real player). `channel` can be `say`
  (the default), `yell`, `party`, `raid`, or `whisper` with `to`. A bot says at most one of these every `BotLineGapMs` (8 s).
- `delay_ms` (optional, 0 to 10000) is how long the bot waits before the text shows: the time a person needs to type it. Send the
  reply as soon as you have it and put the typing time here; an action in the same reply happens at once and only the text waits.
  Without `delay_ms` the server waits 30 ms a letter, at most 2.5 s.
- The text gets cut to `MaxLineBytes` on a word, keeps only its first line, and loses `|` characters.
- `ping` only says the program is still running. Changing the file's time (touch) does the same without adding a line, and is what the example does.
- A line the server can't read is skipped, with one warning a minute in `Server.log`. Unknown fields are ignored, so newer programs
  still work with an older server.

With actions off (the default), only text comes back: nothing in `replies.jsonl` can make a bot do anything except say a line.

**Chat goes to your program only while it runs.** The server sends `chat_in` lines to the bridge (instead of answering itself)
only when `replies.jsonl` was written to (or touched) in the last `ClientTimeoutSec` (120 s). Ping or touch it every 30 s or so. If your
program stops, the bots go back to their usual replies (or to `Playerbot.ChatModel`, if that's on) within two minutes.

## Actions: bots doing what the reply says

Off by default. Turn it on with `Playerbot.Bridge.Actions.Enable = 1` (the bridge must be on too). A reply line may then carry
**one** action in `do`, next to the text or alone:

```
{"reply_to":1042,"say":"Right behind you.","do":"follow"}
{"reply_to":1042,"say":"On it!","do":"attack","target":"speaker_target"}
{"reply_to":1042,"say":"On it!","do":"attack","ref":2}
{"reply_to":1043,"say":"Let me grab that.","do":"accept_quest","quest":416}
{"bot":"Brannor","do":"go_to","ref":1}
{"bot":"Brannor","do":"go_to","x":-5340.1,"y":-2950.3}
{"bot":"Brannor","do":"emote","emote":"wave"}
```

| `do` | What the bot does | Fields and checks |
|---|---|---|
| `follow` | follows a player | `target`: `speaker` (the default, the one who wrote the chat_in) or `owner`. Within 50 yards, same map. A world bot following someone who doesn't lead its group lets go after `FollowMinutes` or 100 yards. |
| `stay` / `resume` | stays where it is / goes back to its own plans | |
| `attack` | attacks a creature | `ref` of a `hostile` from `near`, or `"target":"speaker_target"` (what the player has selected). Only a creature within `NearYards` (60), at most 5 levels above the bot, not friendly, not a peaceful NPC (vendor, trainer, quest giver, civilian), not already tagged by someone else. Never a player or a player's pet. |
| `stop_attack` / `flee` | stops fighting / runs to a player | `flee`: `target` like `follow`. |
| `go_to` | walks somewhere | `ref` (a quest giver, NPC or object from `near`), `"target":"speaker"`, `place`, or `x`, `y` (and `z`). `place` names a place of the zone: a role (`inn`, `mailbox`, `flight master`, `vendor`, `repair`, `bank`, `trainer`, `stable`, `auction house`, `graveyard`), maybe with an area (`"inn at Goldshire"`), or an NPC's, object's or area's name (`"Goldshire"`); the nearest one within `MaxGoToYards` is used. Same map, at most `MaxGoToYards`, a walking path must exist. Never a teleport, flight or ship. |
| `accept_quest` | takes a quest | `quest` (id). Only a quest someone close offers this bot right now (`near` lists them). |
| `turn_in` | hands in finished quests | `quest` optional. |
| `drop_quest` | abandons a quest | `quest` (id), only one in its log. |
| `share_quest` | shares quests with its group | `quest` optional. |
| `unstuck` | nudges 5 yards forward | one per 2 minutes. |
| `sit`, `stand`, `mount`, `dismount` | | `mount`: outdoors only. |
| `emote` | plays an emote | `emote`: wave, hello, bye, bow, thank, cheer, laugh, dance, salute, point, yes, nod, no, cry, roar, flex, kneel, applaud, shy, talk, question, exclamation, rude, chicken, beg, shout, eat. One per 10 s. |
| `order` | one of the bot's own orders, as if the player typed it | `order`: exactly one of follow, stay, hold, resume, come, disengage, flee, attack, assist, mount, dismount, sit, stand, release, passive, defensive, aggressive, accept, accept all, turn in, share, quests, quest list, loot, loot all, sell grey, repair, equip upgrades, upgrades, use your best gear, gear, inventory, stats, who, buff, heal, jump, wave, salute, dance, pull, pull back, fall back, back, rally here, spread, spread out, stack, tight, close, max dps, save mana, normal; or `use <object>`, `drop quest <name>`, `equip <item>`, `craft <item> <count>`, `formation <shape>`, `spacing <tight/close/spread>`, `distance <yards>`. Only for the bot's owner or the player leading its group, only answering his chat_in; the order code answers him as for his own whisper. Also in a dungeon and in a fight (the order code decides). |
| `trade` | opens a trade window with the player and puts things in | `items` optional: what he asked for (words of an item's name, or `food`, `potions`, `bandages`, `bags`, `gear`); left out or `stuff`: what helps him (his quest items, gear that is an upgrade for him, empty bags, then food, drink, potions and bandages for his level). At most 6 stacks from the bot's bags, nothing soulbound. Only for the bot's owner or the player leading its group, within trade range, not in a fight. He accepts in the window; the bot accepts once he has. Never gold. |
| `sign_charter` | signs the guild charter the asking player carries | nothing more. The same as his Request Signature: the bot must have no guild (and no open invite), be of his faction (unless the realm allows both), not be getting its own charter signed, and the charter must have room; within 30 yards. His charter window shows the new name. |
| `step` | moves a few yards and waits there | `dir`: `back`, `forward`, `left`, `right` as the asking player faces (his left is the bot's left), or `away` (from him), `closer` (to him, never nearer than 2 yards); `yards` 1-20, default 5. The spot is the first one free of walls on that line and must be walkable. Out of a fight the bot then stays there (3 min, or until the next order); in a fight it holds there 8 s and fights on. In a dungeon or a fight only for the bot's owner or the player leading its group. |
| `cast` | casts one of its spells | `spell`: a name from the chat_in's `spells` list (case and "Rank N" don't matter). By kind: `damage` needs `ref` of a hostile from `near` or `"target":"speaker_target"`, with the same checks as `attack`; the bot goes at it and casts once in range and sight (gives up after 15 s). `heal`, `buff`, `resurrect`: `target` `speaker` (default), `self` or `owner`; friendly players only, in range and sight; a heal only on someone hurt, a buff not on someone who has it, a resurrect only on the dead. `self`: on the bot. `campfire` (Basic Campfire, Cooking): where the bot stands, outdoors. The spell must be ready. |

Nothing else exists. There are no other commands, no teleports, gold, mail, auction, bank, guild or group changes, duels, or
attacks on players, and no trades except `trade` with the bot's own owner or group leader. A `do` with any other word is refused.

**Who can make a bot act**

- A companion (a player's own bot, `"owned":1` in its chat_in) acts only when its **owner** wrote the chat_in. A stranger
  chatting with someone's bot can't move it. Your program can't act for a companion on its own unless the realm sets
  `Actions.OwnedBots = 1`.
- A world bot in a group with a real player takes anything except emote, sit and stand only from the player leading that group.
- A world bot on its own acts for any player who talks to it, and your program may act for it without a chat_in (`"bot":...`).
- A chat_in written by another bot can only lead to an emote, sit or stand.
- `order` and `trade` only for the bot's owner or the player leading its group, only answering his chat_in. So is `step` in a
  dungeon or a fight.
- In a dungeon, raid, battleground or arena: only follow, stay, resume, emote, sit, stand, order, trade, step. The bots' own logic leads there.
- In combat: only attack, stop_attack, flee, follow, stay, cast, order, step.

When the asking player could give that order himself (the owner, or the group's leader), the action runs through exactly the
same code as his whisper (for example `follow` keeps the dungeon autopilot waiting, as a whispered follow does).

**What the program gets to choose from.** With actions on, `chat_in` has `owned`, `from_is_owner`, `speaker_target` and
`speaker_target_kind` (player, hostile, friendly, neutral), `can` (the actions this player may ask of this bot right now, after
all the rules below: `["follow","stay","emote",...]`, empty when nothing), `spells` (what `cast` may use:
`[{"name":"Fireball","kind":"damage"},{"name":"Arcane Intellect","kind":"buff"},...]`, at most 30, the highest rank of each),
and `chat_in` and `stuck` have a `near` list:

```
"near":[{"ref":1,"kind":"quest_giver","name":"Mountaineer Kadrell","dist":12,"quests":[{"id":416,"name":"Rat Catching"}]},
        {"ref":2,"kind":"hostile","name":"Tunnel Rat Scout","level":12,"dist":18,"count":3},
        {"ref":3,"kind":"npc","name":"Innkeeper Hearthstove","level":30,"dist":25},
        {"ref":4,"kind":"object","name":"Mailbox","dist":9}]
```

Up to 3 quest givers and 2 other NPCs within 40 yards, 3 named objects (a mailbox, a bench, a campfire) within 30 yards, and the 3
nearest kinds of hostile within `NearYards` (60): each kind once, by its nearest one, with `count` when there are more. A `ref` stays valid for 60 seconds; after that, or when the
thing is gone, an action naming it is refused. Game ids never appear, so a model can't invent a target.

**Limits** (config, `Playerbot.Bridge.Actions.*`): one action per bot every `BotGapMs` (5 s), `MaxPerSec` (20) for the whole
server, and an action older than `MaxAgeSec` (30 s) is dropped. A bot with 10 refused actions in a minute takes none for 5
minutes. `Allowed` turns single actions off (`"follow,stay,emote"`, default `all`).

Every action gets an `action_result` event, so a model can say something that fits or try something else:

```
{"id":...,"type":"action_result","bot":"Brannor",...,"act":"attack","why":"someone else is already fighting it","ok":0,"reply_to":1042}
```

## After a crash

Turn on **Auto-restart** in the launcher (the button on the Server panel, off by default), or start the server with
`AutoRestart-WoWForever.ps1`. It starts the server again 15 s after a crash, and gives up after 5 crashes in 10 minutes. The
first event of the new run is a `start` with `"unclean_stop":1` and the path of the newest crash report, so your program
can read it and send it on.

## Example program

`tools/bot_bridge_example.py` (Python 3, nothing to install) prints bot problems, answers every `chat_in` with a fixed line and
sends the pings. Replace its `answer()` with a call to your own model.

`tools/bot_bridge_ollama.py` (Python 3, nothing to install) lets a language model answer. It is free by default: it uses a local
Ollama (`http://127.0.0.1:11434/v1`, `llama3.2:3b`, no key) through the OpenAI-style chat API, so any other service that speaks
that API works too (LM Studio, Open WebUI, a model on a second PC; `--url`, `--model`, `--key` or `BOT_BRIDGE_API_KEY`).
- Each bot gets a persona that never changes (temper, way of writing, how chatty), remembers what players told it, and has an
  opinion of each player that goes up or down with how they treat it. A bot may refuse, tell someone it dislikes to get lost,
  or ask back. All of it lives in `BotBridge\minds\`, one small file per bot.
- Each `chat_in` goes to the model with the persona, the opinion, the memories, the last lines of the conversation, the weather,
  `near`, `spells`, `can` and `speaker_target`. The model's JSON answer (a line, at most one action, how its opinion changed,
  something to remember) is checked against the event before it is written: actions not in `can`, refs not in `near`, spells
  not in `spells` and quests nobody offers are left out.
- Lines that give the bot away as a program, slurs, threats and sexual lines are never said. Swearing is kept only when the
  chat_in says `swearing`, otherwise it is masked the way the game's filter does (`f***`); `--no-swearing` masks it always.
- An answer waits as long as a person needs to type it (`--no-typing-delay` turns that off), with now and then a small typo.
- When the model isn't running or is too slow, the bot gets a short fixed answer.
Which model fits which PC (Low llama3.2:3b, Medium llama3.1:8b, High qwen2.5:14b, no graphics card llama3.2:1b, all free) is
in the player guide (`BOT_BRIDGE_PLAYERS.txt`).
- Thinking models are asked to answer at once (`reasoning_effort: "none"`; a service that refuses the field is asked
  without it). Left to think, they spend the whole answer on hidden reasoning and the content comes back empty.
Options: `--url`, `--model`, `--key`, `--workers` (model calls at once, default 2), `--timeout`, `--no-actions`, `--no-swearing`,
`--no-typing-delay`, `--minds`, `--think` (let thinking models think), `--log FILE` (also write what it prints to a file).

In the repack both programs, this guide, the player guide (`BOT_BRIDGE_PLAYERS.txt`, as `README.txt`) and
`tools/bot_bridge_models.txt` (as `models.txt`) are in `repack\bot-bridge\`. The launcher's AI BOTS tab (`repack/launcher/AiBots.cs`,
`AiBotsTab.cs`) runs the program for players: it takes its tiers from `models.txt`, keeps its choice in `config\ai-bots.txt`,
writes the bridge lines into the player's `zz-9-local.conf`, and starts the program with the launcher's own Python
(`ai-bots\python`, python.org's embeddable package, SHA-256 checked against python.org's own list) whenever the realm runs;
`BotChat.ps1` does the same at the realm's start and ends it at the stop.
