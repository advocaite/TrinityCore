# WoW Forever Bots (folder `ForeverBots`)

A small Classic 1.60 addon with buttons for the server's `.bot` commands. It only sends ordinary chat, so it
does not depend on the addon-message protocol PlayerbotControl uses.

Install: copy this folder to `<client>\_forever_\Interface\AddOns\ForeverBots` (the folder name must stay
`ForeverBots`). If the character screen's AddOns list marks it out of date, tick "Load out of date AddOns".

Languages: the windows, menus, tooltips and chat lines follow the client language. `Locale_ruRU.lua` holds the Russian text
(proofread by Tornn), keyed by the English text; anything missing there shows in English. Spell, class and raid marker names come
from the client itself. The words sent to the server (orders, `.bot` commands) and typed `/fb` words stay English, and what the
bots answer comes from the server in English.

In game: `/fb` opens the window. The Raid ready button (or `/fb raidready`) calls your saved bots (`.bot summon saved`) when you are alone (already in a group, it uses that group and never changes it), makes the group a raid if needed, sets formation and spacing, and starts a ready check once the bots have buffed.

On login the add-on greets the server once (a self-whisper under the `FBOTS` prefix). A player whose client does not greet within 30 seconds gets one chat line with the download link (`Playerbot.AddonHint.*` in the module config).

In game: `/fb` opens the window. The window is grouped under headings: Orders to the group (move and fight, wipe and
regroup, formation and spacing, tanks and markers), Assign a bot to an icon, Your bots. Hover any button for what it does.
- "Add bots >" at the top opens a separate drop-down beside the window: "Select gear" (Match my gear, Best for their level,
  Best in slot, Tier 1, 2, 3; remembered; sent to the group as `gear match|best|bis|t1|t2|t3` before an add) and one button per
  class and role.
- Two switches beside it: Area spells (whispers `aoe_all on|off` to one of your bots; it applies to the whole group; off = no
  spell that hits more than one enemy) and CC (group order `crowdcontrol on|off`). The icon shows the state last sent.
- Stance button (0.6.9), three steps: Defensive (the start setting: bots start nothing, fight what you pull or what attacks the
  group), Aggressive (they also attack anything hostile within about 10 yards of you) and Passive (they attack nothing at all, not
  even to defend; healers heal and buffs go on). Click for the next, right-click for the one before. Sent as `defensive`,
  `aggressive` or `passive`, re-sent as `stance defensive|aggressive|nofight` to bots that join. ForeverBotsDB.stance is
  defensive|aggressive|nofight (an older "passive" reads as defensive).
- Ignore target and Clear (0.6.9): your target goes on the list of mobs your bots never attack, even when it attacks you (a beast
  you tame); the button again on it takes it off, Clear empties the list. Group orders `ignore target` / `ignore clear`; the
  server answers in a system line. Secure buttons, so they work in combat.
- Arena (0.6.11), in the minimap button's menu: Arena 2v2, Arena 3v3 and Leave arena queue send `.bot arena 2|3|leave`. The
  party leader queues the party; 2v2 needs a party of exactly 2 and 3v3 of exactly 3, your bots count. The other team is a real
  party or a bot party the server makes. The same lines can be typed in chat.
- Arena ready pop-up: when your arena is ready a small window shows Enter and Leave (the game's own arena pop has Enter only).
  They send `.bot arena enter` and `.bot arena leave`; Leave also takes your party's bots out of the same queue.
- One button per class and role: adds that bot (`.bot add <class> <role>`). It joins you at your level, geared,
  in your group.
- Hired bots only (0.6.10, the server's Playerbot.Hire.Mode): when you must hire, the class and role buttons are hidden and a
  line says where to hire bots instead (HIRE_REQ / HIRE_RESP, ForeverBots_Hire.lua). Quick party, Call saved and Raid ready stay.
- Orders to the group: Follow, Stay, Attack (your target), Resume; Pull (your tank bots open on your target, the
  rest join once it is engaged), Flee (break off and run to you), Come (teleport to you), Stop (stop attacking and
  return to normal), Release (after a wipe: every dead bot releases its spirit, runs back to its corpse as a ghost,
  into the instance through its portal if it died inside, revives there and rejoins you; living bots ignore it).
  They go out in party/raid chat, so typing the word there does the same; `assist` = attack. Whispering `release`
  to one bot releases only that bot.
- Follow holds until you hand the fight back: while it stands the bots follow you and only defend; they do not
  advance or pull on their own (in a 5-man or a raid) until you say Attack, Pull or Resume, or attack something
  yourself. A party that is not a raid group never runs the raid autopilot in a raid instance: it follows you and
  defends.
- Pull back (`pull back`, `pullback` or `pull and back` in party/raid chat, the Pull back button, `/fb pullback`): your
  main tank bot (a raid's Main Tank flag, else a tank bot) opens on your target (else the skull) from range with a
  throw / shoot / Faerie Fire / Judgement, runs back to the rally point (Set rally) or, with none, to where it
  stood, and tanks the mob there so nothing else aggros. Other bots ignore it. Gives up after 30 s.
- Fall back (`fallback`, `fall back` or `back` in party/raid chat): every bot drops its target and
  runs to the rally point - the spot set with Set rally (`rally here`), else where you stand. Combat does not stop
  the run; healers keep healing with instant spells on the way. There they re-form: tanks in front facing the way
  they came, melee just behind, ranged and healers on an arc behind them. They fight only mobs that follow them
  there (within about 15 yards of the rally, or hitting someone at it) and chase nobody past about 20 yards. They
  stay there until Attack, Pull or Resume (Follow switches back to following you).
- Formation (Circle, Arrow, Line, No form.; `formation circle|arrow|wedge|line|none` in party/raid chat) and spacing
  (Tight about 2.5 yards, Close about 4, the default, Spread about 8; `spacing tight|close|spread`, also `tight`,
  `close`, `spread out`): out of combat the bots follow a slot round you (circle round you, arrow pointing the way you
  face, line abreast behind you; tanks in front, healers at the back; slots are stable per bot and turn with you).
  None is the plain follow. It stays until you change it. In combat and in raid mechanics the raid plan's positions
  win; the spacing also scales the raid ranged / pre-pull spacing.
- No World bots tab (0.6.13): how many world bots the realm has is set in the launcher (Bots: Off, Low, Normal, High) before the
  server starts, nowhere in game.
- Positions tab (0.6.12, the last side tab, a Defensive Stance shield; 0.6.14 adds the mouse spot, 0.6.17 groups and the Set name): pick a shape (Close, Circle,
  Arc, Far apart, Line, Column, Spear, Arrow) and a gap (Tight about 2.5 yards, Normal 4, Wide 8), tick the bots (All, None,
  Tanks, Heals, Melee, Ranged tick by role; G1 to G8 tick a whole raid group, a second click unticks it; click a name to tick or
  untick it, pages with < and >; all of these mix), then send them: Set (click it, then click
  the ground in the client's green circle: a white skull world marker lands there and the bots form the shape on it, facing away
  from you; a ping on the ground works too, within 30 seconds), Place here (just in front of you, pointing the way you face;
  Circle and Arc go round you; never on your own spot) or At target (at a mob or NPC, facing you; never inside it). The ticked
  bots stay on their spots and walk back to them after every fight until Follow. Tanks take the front, healers the back. Sent
  as the hidden add-on message `PLACE_REQ` (fire and forget); the same as a group order: `place <shape> [tight|normal|wide]
  [here|target|cursor] [names or all, tanks, healers, melee, ranged, dps, group1..group8, a class]`, `place follow [who]`. The
  server answers in one system line.
- Distance (the - / + stepper, 1 to 20 yards, one per click; `distance <n>` / `follow distance <n>` in party/raid chat, `/fb distance <n>`):
  how far the bots follow you out of combat; it also sets the formation spacing to the same yards. Saved in the addon
  (ForeverBotsDB.distance) and re-sent silently on login and when the group changes; the server keeps it per bot.
  Raid combat positions are unaffected.
- Main tank (`set main tank <name>`, `main tank <name>`; no name = your current target): sets the raid's Main Tank
  flag on that raid member, the same as the raid frame's right-click. Raid groups only.
- Tank markers (raids): the bot main tank marks the mob it pulls with skull and each off tank marks its own mob (or
  the pack mob reserved for it) with the next markers (cross, star, circle, diamond, triangle, moon, square), and the
  raid's DPS kills skull first, then the next markers in that order. The marks come off when the pack is dead. To
  override, give a tank a marker yourself: the marker button cycles the marker, "Tank target" gives it to the bot
  tank you have targeted (`tank <name> <marker>`; `tank <name> clear`, "Clear tanks" = `clear tanks`). That tank
  then holds whichever mob carries the marker (mark it with the raid icons yourself), instead of the automatic split.
  Switch off with `Playerbot.Behavior.TankMarkers = 0`.
- Assign a bot to a raid icon (the client cannot mark a friendly bot, so the panel does it): target a bot and click
  one of the eight icons in the "Assign a bot to an icon" section (or type `assign <name|target> <marker> [tank|cc|kill]`
  in party/raid chat, or `/fb assign <name> <marker> [role]`). Mark the mobs yourself with the normal raid target menu;
  the bot follows its icon. Role auto: a tank bot tanks the mob carrying that icon, a crowd-control class (mage, warlock,
  priest, hunter, rogue, druid, paladin) crowd-controls it until a skull goes on it, any other bot kills it first. The
  Role button (auto, tank, cc, kill) overrides that for the next assignment only (skull cannot be crowd-controlled).
  right-click an icon clears it (`assign clear <marker|name>`), Clear all clears everything (`assign clear`). A bot holds
  one assignment; a tank icon belongs to one tank. The list under the icons shows what the server holds (icon, bot,
  role); it asks the server (`assign list`) when the panel opens, after each click and every 5 seconds, and one bot
  answers with hidden whispers `ASSIGN|<marker 0..7>|<name>|<role>` ... `ASSIGN|END|<count>`. `/fb assign list` also
  prints it in chat. Assignments live in memory until cleared (not saved across a server restart). Needs
  `Playerbot.Behavior.TankMarkers` (tank, kill) and `Playerbot.Behavior.MarkCc` (cc).
- Bots release on their own too: when you release (they run back with you and revive next to you), and after a
  wipe with nobody left alive in the instance. While a group healer (or you, as a priest, paladin, shaman or druid)
  is alive next to their bodies they wait for a rez instead. After reviving they rebuff and eat/drink; a bot main
  tank does not start the next pull while anyone is dead or a healer is under 80% mana.
- Your bots: Call saved (`.bot summon saved`, same as `.bot call all` but open to normal players: the bots on your saved list and every bot in your group), Save group (`.bot save all`: every
  bot with you goes on the list), Dismiss all, Unsave all (empties the list), Delete target (asks first, then
  deletes the targeted bot's character for good), List (`saved` marks the ones on the list), Regear all (`.bot regear
  all`: the same bots as Call saved, online ones only, get their gear re-rolled so they don't all wear the same
  pieces; better pieces they already wear stay, replaced ones go to their bags).

Per-bot control (whisper or the floating panel): every order that works in party/raid chat also works as a whisper to ONE
bot (`/w Raveni follow`, `stay`, `attack`, `pull back`, `distance 8`, `range 8`, `rti skull`, `tank star`, `assign star tank`,
`assign clear`). A whisper acts on that bot only, that bot always answers with a confirming whisper such as `Raveni: tank
on Star`, `Raveni: attacking <target>`, `Raveni: following at 8 yards`. `assign <marker> [tank|cc|kill]` with no name is the
whispered bot itself. Only the bot's owner may whisper it orders. When your target is a friendly player of your group, the
small ForeverBotsBot panel appears next to the target frame (drag it; the position is saved): the eight icons (click assigns
the bot, right-click clears; the Role button picks tank/cc/kill for the next click, default auto), Follow, Stay, Attack
(your target), Pull back, Dist -/+ (this bot only), Dismiss, and a status line showing the bot's confirmation. The
confirmation whispers are hidden from chat. `/fb bot on|off` toggles the panel (default on). The client cannot tell a bot
from a human group member, so the panel shows for any friendly party/raid member (a `?` after the name until that player
has answered a whisper order); do not use it on real players. Unit right-click menu: entries are added only through the
client's `Menu.ModifyMenu`; the legacy UnitPopupMenus tables are deliberately not touched (`/fb menu off` leaves the menus alone).
Right-clicking a member of your group shows "WoW Forever Bots" with a submenu, each entry a whisper to that bot:
- Open the bot panel, Follow me, Stay here, Attack my target; Area spells on / off for this bot (`aoe on|off`); Role (`setrole tank|healer|dps|clear`) for classes with more
  than one role; Raid icon: kill it first, crowd-control it, tank it (`assign <marker> kill|cc|tank`), Clear.
- The bot's own picks (`prefer <slot> <name>`, `prefer <slot> auto`): paladin Blessing and Aura, shaman Earth / Fire / Water /
  Air totem, hunter Aspect (Cheetah and Pack only out of combat), warlock Demon. A pick replaces the group plan for that one
  choice; the other paladins plan around a pinned blessing or aura. A mark shows what you picked in this session.
- Pet (hunter, warlock): keep it out or dismiss it and keep it away (`pet on|off`). Stealth (rogue): `stealth on|off`.
- Warlock: Summon a player (`ritual <name>`, then "Help click the portal" sends `use summoning` to the party) and Soulstone
  on a group member (`soulstone <name>`). Mage: Open a portal to a city of the mage's faction (`portal <city>`).
  Mage: Conjure water / Conjure food (`craft water x20`, `craft food x20`): the mage conjures them and asks you to open a trade
  with it, then puts them in the window (conjured items cannot go by mail).
- Danger spells (all bots of that class): the crowd-control orders `@<class> no <kind>`, `@<class> off cc`, `@<class> on cc`.
- What did I pick? (`prefer`) and Clear my picks (`prefer clear`).
Class entries appear from the level the spell is trained at; a talent spell the bot lacks gets "I don't know ... yet". The
picks are memory only on the server: a relog or restart clears them.

`/fb add <class> [tank|healer|dps]`, `/fb call|dismiss|save|unsave|regear [name|all]`, `/fb delete <name>`, `/fb list`,
The window stays where you put it and stays open (it is not closed by Escape, clicks, combat or target changes; its open state survives /reload and relog). `/fb` toggles it, or use the X button.

In a fight: the side tabs and the X are secure buttons, so every tab (Orders included) and closing work in combat. A second window key, "Show / hide the bot window (works in combat)" in Key Bindings, opens and closes it in combat too. `/fb`, the minimap button, the plain toggle key, dragging and `/fb scale` cannot touch the window in combat (the client keeps it fixed because of its secure order buttons): they wait until combat ends, with one chat line saying so (0.5.16).

`/fb fallback`, `/fb pullback`, `/fb rally`, `/fb formation circle|arrow|line|none`, `/fb spacing tight|close|spread`, `/fb distance <1-20>` and `/fb maintank [name]` and `/fb tank <name> <marker>|clear` work without the window.
The `.bot` commands for your own bots (add, call, dismiss, save, list, regear and the rest) work for every account; no GM account is needed. Only `.bot world`, `.bot bgstress`, `.bot bgsoak` and `.bot itemstats` are GM.

Timings (0.5.18): every handler the add-on gives the client is timed. A call over 15 ms prints one chat line naming it; `/fb perf` lists the slowest (`/fb perf reset` clears), and the list is saved as `perf` in the SavedVariables file at logout. `/fb part` lists the optional parts that run inside client code (menu, filters, reporter, bindhook); `/fb part <name> off` and `/reload` switch one off.

Key bindings (0.2.2): Esc > Options > Key Bindings > AddOns > "WoW Forever Bots". Unbound by default; one entry each for Attack, Pull, Pull back, Follow, Stay, Resume, Stop attacking, Flee, Come, Fall back, Toggle stance (Next stance since the stance button got three steps), Show / hide the bot window, and one key each for the stances Passive, Defensive and Aggressive and for Ignore my target and Clear ignored mobs. The order keys send the same line as the window buttons and work in combat.

Lazy lock key (0.2.4, warlocks): one key that does the right next thing on your target. Each press casts one spell at an enemy, never a pet summon: the curse (Curse of Tongues on mana users, Curse of Agony on the rest), Corruption, Immolate and Siphon Life kept up, Conflagrate, Shadow Bolt as the filler (the free instant bolt on a Shadow Trance proc), Life Tap when mana runs low, Death Coil / Drain Life when your health is low. An enemy hitting you in melee gets Death Coil, then Howl of Terror when two or more are on you, then Fear (not on bosses, and not on mobs in dungeons and raids). Both on-use trinkets and your racial go whenever ready on a long fight, in the same press as the next spell. The wand takes over when mana is dry. Press it with no enemy selected to put Demon Armor up and summon your pet. It also sends your pet at the target. Bind it under "Lazy lock" and tap or mash it; it runs the server command `.playerbot lazy`, which you can also type.
Attack (mouseover) key: hover a mob and press it; the mob becomes your target and the bots attack it. Group pings (the ping wheel, if your client has one): an Attack ping on an enemy sends the bots in, an On my way ping on the ground sends them there. Needs a server with the ping hooks.

## 0.5.1 window

Built on the client's own portrait window (the one its Raid and Looking For Group windows use): centred title, group icon in the portrait slot, dark inner panel, the client's own red buttons, fonts, and square icon tabs down the right edge (Orders, Bots, Form, Marks; hover for the name). Bots tab: class icons, then round role icons (shield, plus, sword) for the roles that class can do; click one to add that bot. A footer shows the group and stance; Call saved and Dismiss all sit in it on the Bots tab only (0.6.17). If the client has no such template the window falls back to a plain dialog frame with the same contents. The only own art is the minimap emblem (`Textures/mono_fb64.tga`); buttons are the Raid Info button template, role icons use the client's LFG art (no check box since 0.6.15: one click on the icon adds the bot), switches are client check boxes, mark slots are the client's action-slot frame.

- `/fb scale <0.7-1.5>` sets the window size (default 1.2, saved).
- `/fb minimap on|off|reset` shows, hides or re-centres the minimap button.
- `/fb quick` fills your group with bots: to five in the open world and 5-player dungeons, to the instance size in a bigger one (10 in Upper Blackrock Spire, 20 or 40 in a raid), with tanks, healers and damage dealers; `/fb resume` and `/fb flee` send those two orders.
- Group finder tab (the fifth tab, after Marks): the bot dungeon finder without typing. Pick your role (Auto, Tank, Heal, DPS;
  remembered) and a dungeon with < and > (Any for my level, then the dungeons of your level, then the rest; Upper Blackrock Spire
  from 58), then Queue: your bots fill the missing places at your level, in the gear picked on the Bots tab, and the group is
  put inside the dungeon. Cancel stops a queue that is still forming, Leave dungeon takes you back to where you queued, Fill my
  group fills the party here (`.bot lfg`). The status line (queued, how many of how many, the wait, inside) asks the server
  every 2 seconds while the tab is open. All of it goes as hidden add-on messages. `/fb dungeon <name|any|leave|cancel>` still
  works typed; `/fb dungeon` lists the dungeons.
- When the client blocks an add-on call, one chat line names the add-on and the call.

## 0.6.0 bot manager

`/fb bots` (also `/fb manage`, `/fb roster`) or "Manage bots" on the Bots tab opens a second window. It talks to the server
with hidden add-on messages (prefix `PBC`, the bot module's AddonControl), so nothing it asks for or sends shows in chat;
the bots' confirming whispers stay hidden as before. `/fb ping` checks that the server answers (an older server does not). The answers come as hidden add-on whispers, or, on a client that drops those (Classic 70658), as system lines starting `PBC~` that the chat frames hide: the add-on finds which works a few seconds after login and the server remembers it for the account. The requests (and the greeting and queued `.bot` lines) go out on the party or raid channel when you are in a group, because Classic 70658 add-on whispers never reach the server; alone they are whispers to yourself.

- Left: every bot you own, online or not, in your group first, with class, level and role. The mouse wheel scrolls.
- Details: spec, role, where it is, health, mana, experience and what it is doing. Call (offline) / Summon (online),
  Dismiss, Save / Unsave, Regear, Delete (asks first): the same `.bot` lines as the main window, sent hidden.
- Gear: the 19 worn slots and the bags (best first), with the real item tooltips and the item level on this bot. Click a
  bag item to have the bot wear it, shift-click to link it. Equip upgrades, Sell grey, Repair.
- Quests: the quest log with each objective's progress. Click a quest, then Drop (asks first) or Share; Accept and Turn in
  work on the quest givers next to the bot.
- Spec: spec and role; a button per role the class can play (`setrole`), From talents (`setrole clear`), Re-pick talents
  (`talents apply`).

The window reads the roster every 5 seconds and the open page every 2 (Details), 5 (Quests, Spec) or 10 (Gear) seconds
while it is open. Files: `ForeverBots_Net.lua` (requests), `ForeverBots_Windows.lua` (window, roster, Details),
`ForeverBots_Gear.lua`, `ForeverBots_Quests.lua`, `ForeverBots_Spec.lua`.
