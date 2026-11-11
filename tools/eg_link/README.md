# EG Link tools

Efficient Games publishes what players of the live WoW Forever realm record with their EG Link app and addon:
https://github.com/JIVESCORP/eg-link-output-wowf (CC BY 4.0). One folder per client build, `data/wowf/<build>/`.

How they get it: players install the EG Link Windows app, which adds an addon to the game. The addon writes what the game
shows (vendor and trainer lists, quest givers, loot windows, flight routes, character sheet, tooltips) into its saved variables.
The app uploads those, the game's own cache files (`Cache\WDB` quest, creature and object caches, `Cache\ADB\DBCache.bin`)
and, if the player turns it on, the combat log. Nothing is read from network packets.

1. `eg_compare.py` compares one build folder with our world database (read only) and writes `compare.json`.
2. `eg_sql.py` turns that into two SQL drafts: the live data file and the trainer prices file.

Their `hotfixes` and `item_hotfixes` datasets come from players' `DBCache.bin`, the same records as Blizzard's public hotfix file
we already read with `tools/full_forever/live_hotfix_sql.py`, so they are not used here.

Credit to keep with anything made from it (NOTICE.md has it):
Data from Efficient Games (https://efficient.games), licensed under CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/).
