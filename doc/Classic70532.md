# WoW Classic beta 1.60.1.70532 (preliminary)

Status: **preliminary**. Full protocol compatibility is **not verified** (see Known limitations).

## What changed in 70532

70532 (built 2026-10-20) is a small update of 70450 (built 2026-10-15). Compared through the Blizzard CDN
(`wow_classic_beta`, build config `ad1802f4455e427ed8b7769aab25b0c0` vs 70450 `84bea4233331e390fda8724c6f91ffa5`):

- CASC root: 87 files changed, none added or removed: 17 DLLs (exception handlers, `WowB_loader.dll`, `wow_loader.dll`),
  52 shaders and 18 interface files (15 Lua, 3 XML). No DB2, model, map or other game data changed, so the 70450 client data (and
  `data/dbc` extracted from it) still applies.
- Interface changes are client-side only: the gamepad "toggle tooltips" binding and its `GamepadDisableTooltips` CVar
  were removed, the guild invite frame got gamepad focus handling, a quest with a single reward choice preselects it,
  legacy talent buttons and the swing timer's out-of-range display were restyled. No new client API or event is used.
- macOS client string table: unchanged except the build number, dates, the removed `GamepadDisableTooltips` CVar and one
  internal type name.
- `WowB.exe`: the same set of RTTI types as 70450 (1469; only compiler-generated anonymous namespace hashes differ).

The server picks the Classic opcode group shift and update field layouts with `realmlist.gamebuild >= 70291` and
`>= 70425`, so a realm on 70532 uses the 70425/70450 protocol. No code change was needed for 70532 itself.

## Installation

`sql/custom/auth/2026_10_24_00_auth_classic_beta_70532.sql` is applied by worldserver's database updater on its next
start, like every other `sql/custom` file. It adds build 70532 to `build_info` and moves the Forever realms 70-73 from
70450 to 70532, like every earlier client build did. Realms with other ids or on other builds are not touched.

**After the update the realms 70-73 only accept the 70532 client.** A realm only lists for and accepts clients of its
exact `gamebuild`, so every player has to update to 70532.

- Fresh installation: nothing to do by hand.
- Existing installation: restart bnetserver once after worldserver applied the update; it reads `build_info` only at
  startup, and until then it does not know 70532.
- To keep a realm on 70450 (or go back), set its `gamebuild` to 70450 again; bnetserver re-reads the realm list every
  `RealmsStateUpdateDelay` seconds, no restart needed. The update is not applied a second time.

Keep `Network.SkipBuildAuthKeyCheck = 1` in `worldserver.conf`: no auth key is known for 70532.

## Known limitations

- The client code is encrypted on disk, so the 70532 packet readers could not be compared with 70450, and there is no
  official 70532 sniff yet. Opcodes, packet layouts and update fields are assumed unchanged based on the evidence above.
- The client has to be started with a launcher that supports 70532; launcher support is outside this repository.
