# Notice and credits: WoW Forever Bots (this repository)

"WoW Forever Bots" by Karrz is the name of this bot system. Every upstream credit and license below stays in force.

This module is part of a TrinityCore-based tree and is distributed under the GNU General
Public License version 2 (`COPYING`), the same terms as TrinityCore.

- **PlayerbotV2** was written by agatho (https://github.com/agatho/TrinityCore, branch
  `playerbot-v2`), whose README states "GPL-2.0, same as TrinityCore". The core API, the
  module layout and most of the bot brain, travel and fleet code come from there.
- **Classic (1.12) adaptation, raid, battleground, world-population and auction house work**
  was added for WoW Forever by Karrz (Karrz1).
- **ike3 / cmangos playerbots** (ike3's mangosbot, https://github.com/ike3/mangosbot-bots, for cMaNGOS; GPL-2.0): several behaviours
  (raid brain, speech lines, rpg, battleground paths and objectives, travel) were ported from a
  VMaNGOS-based bot tree that derives from ike3 bots, VMaNGOS and cMaNGOS (all GPL-2.0).
  Source comments name the routines they follow.
- **mod-playerbots** (AzerothCore, AGPL-3.0) was read as a reference for behaviour ideas
  (for example dungeon dispersal and fleeing rules). Its code was not copied; the logic was
  re-written here. Comments mark where an idea came from.
- **Vanilla encounter data and scripts** follow VMaNGOS (GPL-2.0), as do the Classic ports
  already in `src/server/scripts/Custom/Classic`.
- **SimulationCraft (simc)** (GPL-3.0) action lists and talent loadouts were used as
  reference for the retail-era rotations and `sql/playerbot_v2/0004_simc_seed.sql`.

Keep these notices, and the copyright and licence headers that individual files carry, when
redistributing. Source files of the module do not carry per-file headers; this notice and
`COPYING` apply to all of them.

## This repository

- This branch is advocaite's TrinityCore `forever` tree (https://github.com/advocaite/TrinityCore, GPL-2.0, copyright the TrinityCore
  project and contributors) plus WoW Forever Bots. The files that were changed are derived from TrinityCore and stay under its GPL-2.0.
- The 1.12 numbers (spell values, creature stats, ...) are read from the VMaNGOS database dump by `tools/classic_spell_values/`. The dump is
  not part of this repository; it belongs to its authors.
- Libraries under `dep/` keep their own licenses (see each folder).
- mod-playerbots (AzerothCore) is AGPL-3.0: used only as a reference for ideas, no code copied. SimulationCraft is GPL-3.0: data used as reference.
- World of Warcraft data, names and client files belong to Blizzard Entertainment and are not included.
- Some live WoW Forever world data (quest texts and objective counts, object templates, vendor items, trainer levels and prices) in
  `sql/custom/world/2026_11_11_10_world_forever_eg_link_live_data.sql` and `2026_11_11_11_world_forever_eg_link_trainer_prices.sql`:
  Data from [Efficient Games](https://efficient.games) (https://github.com/JIVESCORP/eg-link-output-wowf), licensed under
  [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). Changed: only the records we lacked or had different were picked and
  converted to our database tables.
- Live WoW Forever quest data (quest order, givers and turn-ins) in `sql/custom/world/2026_11_12_15_*.sql`, `2026_11_13_86_*.sql` and
  `2026_11_13_87_*.sql`: from [Questie](https://github.com/Questie/QuestieDB) (Questie team, GPL-3.0), its Forever corrections and
  play traces. Changed: converted to our database tables, only where ours lacked it.
- Live WoW Forever world data (quest records, object templates, vendor items, trainer levels) in
  `sql/custom/world/2026_11_12_14_*.sql`, `2026_11_13_88_*.sql` and `2026_11_13_90_*.sql`: Data from
  [Efficient Games](https://efficient.games) (https://github.com/JIVESCORP/eg-link-output-wowf), licensed under
  [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). Changed: only the records we lacked or had different were picked and
  converted to our database tables.
- Quest givers, drops and vendor items in `sql/custom/world/2026_11_13_91_world_forever_everlake_data.sql`: from Everlake's world data
  ([buildthehomelab/wow-everlake-world-data](https://github.com/buildthehomelab/wow-everlake-world-data), GPL-3.0), which in turn
  credits EG Link (CC BY 4.0), Polestar forever-db, QuestieDB, ForeverDB.net (GPL-3.0), NorgannonDB by zmarotrix (GPL-3.0) and Icy Veins'
  WoW Forever database (db.icy-veins.com/wow-forever). Changed: compared with our world and only rows we lacked were written as ours.
