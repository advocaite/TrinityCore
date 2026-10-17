# WoW Classic beta 1.60.1.70450 (preliminary)

Status: **preliminary**. Login and entering the world with a 70450 client were tested successfully on realm 70
(2026-10-17). Full protocol compatibility is **not verified**.

## What changed in 70450

70450 (built 2026-10-15) is a rebuild of 70425 (built 2026-10-14):

- CASC root: 17 files changed, all DLLs (exception handlers, `WowB_loader.dll`). No DB2, model, map or interface data
  changed, nothing was added or removed, so the 70425 client data (and `data/dbc` extracted from it) still applies.
- macOS client string table: byte-identical except the build number and dates.
- `WowB.exe`: the same set of RTTI types as 70425.

The server picks the Classic opcode group shift and update field layouts with `realmlist.gamebuild >= 70291` and
`>= 70425`, so a realm on 70450 uses the 70425 protocol. No code change was needed for 70450 itself.

## Included: crash fix from #62

This branch includes the fix from #62 unchanged (commit `f50de2d773`, merged so it keeps its SHA): the three UnitData
int32 that client 70425 added after `TaxiNodesID` are written only in the owner's own create. Sent to every viewer they
shifted the PlayerData that follows in the create of another player, and clients seeing other players ran out of memory
and crashed. The fix applies to every realm with `gamebuild >= 70425`, so to 70425 and 70450 alike. Once #62 is merged
upstream this commit drops out of the branch's diff.

## Installation

`sql/custom/auth/2026_10_17_00_auth_classic_beta_70450.sql` is applied by worldserver's database updater on its next
start, like every other `sql/custom` file (fresh and existing installations alike). It adds build 70450 to `build_info`
and moves the Forever realms 70-73 from 70425 to 70450, like every earlier client build did. Realms with other ids or on
other builds are not touched.

**After the update the realms 70-73 only accept the 70450 client.** A realm only lists for and accepts clients of its
exact `gamebuild`, and one realm cannot serve 70425 and 70450 at the same time, so every player has to use 70450.

- Fresh installation: nothing to do by hand.
- Existing installation: restart bnetserver once after worldserver applied the update; it reads `build_info` only at
  startup, and until then it does not know 70450.
- To keep a realm on 70425 (or go back), set its `gamebuild` to 70425 again; bnetserver re-reads the realm list every
  `RealmsStateUpdateDelay` seconds, no restart needed. The update is not applied a second time.

Keep `Network.SkipBuildAuthKeyCheck = 1` in `worldserver.conf`: no auth key is known for 70450.

## Known limitations

- The client code is encrypted on disk, so the 70450 packet readers could not be compared with 70425, and there is no
  official 70450 sniff yet. Opcodes, packet layouts and update fields are assumed unchanged from 70425 based on the
  evidence above and the successful login, not on a full check.
- The client has to be started with a launcher that supports 70450; launcher support is outside this repository.
- Known 70425 issues apply to 70450 too, e.g. #63 (Auto Shot arrows drawn as a checkered cube; not fixed here).
