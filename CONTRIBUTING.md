# Contributing to ForeverCore

Thanks for helping. Bug reports, data fixes and code are all welcome. Please read this page once before you start.

## The one rule: Blizzlike, with a source

ForeverCore copies Blizzard's game for the WoW Forever client. A change is welcome when it does one of these:

- makes the server match Blizzard's game, and says **where that comes from**, or
- fixes the server itself (a crash, a wrong packet, pathing, a broken script).

Good sources, best first:

1. Packet captures (sniffs) from the official Forever realms.
2. Client data: DB2 files, hotfixes, the player's `WDB` cache.
3. Questie data, Wowhead (the Forever / Classic pages), in-game screenshots or video.
4. VMaNGOS, CMaNGOS or TrinityCore, for vanilla behaviour that Forever did not change.

Not welcome in the core: custom items, quests, NPCs, rates or features. Things that only some server owners want can be
an option in `worldserver.conf` that is **off by default**, or a module in `src/modules/` (see
[src/modules/README.md](src/modules/README.md)).

## Reporting a bug

Use the [issue forms](https://github.com/Karrz1/TrinityCore/issues/new/choose). Please give:

- the commit you run (`.server debug` in game, or the first lines of the worldserver log),
- the ids of the creatures, quests, items or spells involved, with a Wowhead link if you have one,
- what happens, and what should happen instead (and how you know),
- for a crash: the crash log from the `Crashes` folder.

Search the open issues first; if the bug is already there, add to it instead of opening a new one.

## Sending a pull request

1. Fork the repository and make a branch from `forever` (or from `forever-bots` for a change to the bots module).
2. One fix per pull request. Small pull requests get reviewed quickly.
3. Fill in the pull request form, including the **source** box.
4. Say how you tested it: does it build, did you check it in game.

Please tick "Allow edits from maintainers", so we can fix small things for you.

### C++

- Follow the [TrinityCore C++ standards](https://trinitycore.atlassian.net/wiki/spaces/tc/pages/2130103/C+Development+Standards)
  and `.editorconfig` (4 spaces, at most 160 columns).
- Change core files only when you must. If a feature can be a module or a script, make it one.
- A new option goes in `worldserver.conf.dist` with a description and a Blizzlike default.

### SQL

- World data fixes go in `sql/custom/world/`, named `YYYY_MM_DD_NN_world_<classic|forever>_<short_name>.sql`
  (`classic` for vanilla data, `forever` for what the Forever client added or changed). For an open pull
  request, use today's date; we fix the date when we merge if needed.
- Start the file with a comment that says what it fixes and **where the data comes from**.
- Every file must be safe to run twice: `DELETE` the rows you `INSERT` (by guid and id for spawns), no `REPLACE INTO`.
- Use backticks around table and column names, list the column names in every `INSERT`, and use one `INSERT` per table.
- Don't use database names in the file, and don't `DELETE` + `INSERT` a row just to change one column: use `UPDATE`.
- New spawns: use free guids, and say where the position comes from (sniff, Questie, video).
- Changes to the `auth` or `characters` schema also go into `sql/base/`.

### Credits

If your fix uses another project's code or data, name it in the file and in the pull request, and check that its license
allows it (GPL-2.0 compatible). We keep the author of every merged commit.

## Questions

Ask on our [Discord](https://discord.gg/eJt62dDh54). Please don't ask the TrinityCore team about ForeverCore.
