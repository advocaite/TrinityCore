# Upstream (advocaite/TrinityCore `forever`)

The trunk is built from **one exact upstream commit**, named in the file `UPSTREAM_PIN` at the repository root. Nothing
newer is merged because it appeared; the pin moves only through the steps below. The first 12 characters of the pin are in the
version string and in the worldserver startup self-check (`[startup] ... upstream pin ...`), so a running server always says which
upstream it is.

## Moving the pin

1. **Throwaway branch.** Fetch advocaite's `forever` and merge or apply the commits from the old pin to the candidate into a new
   branch `upstream-candidate-<short hash>` off the trunk. The trunk is not touched.
2. **Check the diff** with `scripts/upstream-diff.sh` (see below): what the candidate changes in files the fork also edits, and
   whether it touches the packet, group, chat or opcode code (the places the 70291 client protocol work lives).
3. **PC build and test.** The PC builds the candidate in the normal build slot and runs `ctest` (the opcode, party-update and
   feature-status layout tests included). Nothing is installed from the candidate, and nothing new runs on the PC in between.
4. **Move the pin** (one commit on the trunk: `UPSTREAM_PIN`, the merge, this file's "last move" line) only when the build and
   every test are green **and Karrz says OK**.
5. Cadence: weekly, or sooner for a fix the fork needs.

## Keeping the hooks small

The bot module (`src/modules/PlayerbotV2`) and the core-side API it drives (`src/server/game/Playerbot`) are the fork's own
code. The less of **upstream's** files they touch, the cheaper every merge is. The call sites inside upstream-owned files are
(`scripts/upstream-diff.sh hooks`; the number is the count of lines that mention the bot API):

| File | Lines |
|---|---|
| `src/server/game/Battlegrounds/BattlegroundQueue.cpp` | 4 |
| `src/server/game/DungeonFinding/LFGMgr.cpp` | 2 |
| `src/server/game/Entities/Player/Player.cpp` | 3 |
| `src/server/game/Entities/Unit/Unit.cpp` | 4 |
| `src/server/game/Guilds/Guild.cpp` | 2 |
| `src/server/game/Handlers/BattleGroundHandler.cpp` | 12 |
| `src/server/game/Handlers/CharacterHandler.cpp` | 2 |
| `src/server/game/Handlers/ChatHandler.cpp` | 9 |
| `src/server/game/Handlers/LFGHandler.cpp` | 2 |
| `src/server/game/Handlers/MiscHandler.cpp` | 2 |
| `src/server/game/Server/WorldSession.cpp` / `.h` | 2 / 2 |
| `src/server/game/World/World.cpp` | 1 |
| `src/server/worldserver/Main.cpp` | 3 |
| `src/server/scripts/Commands/cs_playerbot_v2.cpp` | the `.bot` command table (the fork's own file) |

A new hook goes through `PlayerbotHooks` (one call, no logic in the upstream file) and is added to this table in the same commit.

## What else the fork changes

Not bot hooks, but also edits to upstream files, which a merge has to carry: the Classic 1.60 client layer
(`Server/Protocol/ClassicOpcodes.cpp`, `Server/Packets/*`, `Handlers/*`, update fields), the map and path generation
(`Maps`, `common/mmaps_common`), the Classic raid and battleground scripts, spells and the auction house bot.

`scripts/upstream-diff.sh modified` lists every upstream file the tree edits (about 160 files at the first pin) and
`scripts/upstream-diff.sh added` every file it adds outside the bot module. It needs the pin commit in the local object store
(`git fetch https://github.com/advocaite/TrinityCore forever`).
