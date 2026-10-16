# WoW Forever Bots (folder `ForeverBots`)

A small Classic 1.60 addon with buttons for the server's `.bot` commands. It only sends ordinary chat, so it
does not depend on the addon-message protocol PlayerbotControl uses.

Install: copy this folder to `<client>\_classic_beta_\Interface\AddOns\ForeverBots` (the folder name must stay
`ForeverBots`). If the character screen's AddOns list marks it out of date, tick "Load out of date AddOns".

In game: `/fb` opens the window. The Raid ready button (or `/fb raidready`) calls your saved bots (`.bot summon saved`), makes the group a raid if needed, sets formation and spacing, and starts a ready check once the bots have buffed.

On login the add-on greets the server once (a self-whisper under the `FBOTS` prefix). A player whose client does not greet within 30 seconds gets one chat line with the download link (`Playerbot.AddonHint.*` in the module config).

In game: `/fb` opens the window. The window is grouped under headings: Orders to the group (move and fight, wipe and
regroup, formation and spacing, tanks and markers), Assign a bot to an icon, Your bots. Hover any button for what it does.
- "Add bots >" at the top opens a separate drop-down beside the window: "Select gear" (Match my gear, Best for their level,
  Best in slot, Tier 1, 2, 3; remembered; sent to the group as `gear match|best|bis|t1|t2|t3` before an add) and one button per
  class and role.
- Two switches beside it: AoE (whispers `aoe_all on|off` to one of your bots; it applies to the whole group) and CC (group
  order `crowdcontrol on|off`). The icon shows the state last sent.
- One button per class and role: adds that bot (`.bot add <class> <role>`). It joins you at your level, geared,
  in your group.
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
has answered a whisper order); do not use it on real players. Unit right-click menu: an entry "ForeverBots" is added only
where the client offers `Menu.ModifyMenu` (untested here); the legacy UnitPopupMenus tables are deliberately not touched.

`/fb add <class> [tank|healer|dps]`, `/fb call|dismiss|save|unsave|regear [name|all]`, `/fb delete <name>`, `/fb list`,
The window stays where you put it and stays open (it is not closed by Escape, clicks, combat or target changes; its open state survives /reload and relog). `/fb` toggles it, or use the X button.

`/fb fallback`, `/fb pullback`, `/fb rally`, `/fb formation circle|arrow|line|none`, `/fb spacing tight|close|spread`, `/fb distance <1-20>` and `/fb maintank [name]` and `/fb tank <name> <marker>|clear` work without the window.
The `.bot` commands need a GM account (gm level 3 on the realm).

Key bindings (0.2.2): Esc > Options > Key Bindings > AddOns > "WoW Forever Bots". Unbound by default; one entry each for Attack, Pull, Pull back, Follow, Stay, Resume, Stop attacking, Flee, Come, Fall back, Toggle stance and Show / hide the bot window. The order keys send the same line as the window buttons and work in combat.
Attack (mouseover) key: hover a mob and press it; the mob becomes your target and the bots attack it. Group pings (the ping wheel, if your client has one): an Attack ping on an enemy sends the bots in, an On my way ping on the ground sends them there. Needs a server with the ping hooks.
