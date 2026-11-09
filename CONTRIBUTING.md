# Sending fixes

Thanks for helping! A few things make a fix easy to take in:

- **Blizzlike only.** The fix makes the game match Blizzard's realms, or fixes the server (crash, packet, pathing,
  script). No custom items, quests or rates. Something only some servers want can be a switch in `worldserver.conf`
  that is off by default, or a module in `src/modules/`.
- **Say where it comes from.** Sniff, client data, Wowhead, Questie, a video or screenshot. We need to be able to check it.
- **One fix per pull request**, against `forever` (or `forever-bots` for the bots). Say how you tested it.
- **SQL** goes in `sql/custom/world/` (or `hotfixes/`), named `YYYY_MM_DD_NN_world_<what>.sql`, with a comment at the top
  saying what it fixes and the source. It must be safe to run twice: delete what you insert, no `REPLACE INTO`, no
  database names.
- **C++** follows `.editorconfig` (4 spaces, 160 columns) and TrinityCore's
  [code style](https://trinitycore.atlassian.net/wiki/spaces/tc/pages/2130103/C+Development+Standards).
- Code or data from another project: name it, and it must be GPL-2.0 compatible. Your name stays on your commit.

Bugs go in the [issues](https://github.com/Karrz1/TrinityCore/issues). Questions: [Discord](https://discord.gg/eJt62dDh54).
