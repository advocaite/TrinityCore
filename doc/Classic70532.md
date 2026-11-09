# WoW Forever / Classic beta client 1.60.1.70532 (preliminary, not working yet)

Status: **not verified.** Nothing in this branch changes the live realms 70-73. Only a separate test realm is added.

## What the client changed

70532 is the build Battle.net now offers; 70450 is no longer offered. Another Forever server reports that all DB2,
locale DB2, maps, vmaps, Buildings, cameras and gt files are identical to 70450, so the 70450 game data (and `data/dbc`) should still
apply. This is their statement and has not been checked here.

The server picks the Classic opcode group shift and update field layouts with `realm->Build >= 70291` and `>= 70425`
(`ClassicOpcodes.cpp`), so a realm on 70532 gets the 70425/70450 protocol without a code change.

## Known problem: login fails before the realm (issue #116)

A 70532 client drops the login with `ERROR_NETWORK_MODULE_SOCKET_CLOSED (1016)` a few milliseconds after "Battle.net logon requested";
the launcher's own check reports "SSL Handshake failed, stream truncated"; bnetserver logs nothing. Adding the 70532 `build_info` and
realm rows did not help for the reporter. The cause is not known. Do not guess: capture one login (below) first.

## Test realm

`doc/classic70532/70532-test-realm.sql` adds the `build_info` row for 70532 and realm 74 "Forever Test 70532"
(port 8125, content set 137 like realm 70). Realms 70-73 keep their build, so the 70450 realm stays playable.

**Manual script, deliberately not in `sql/custom`:** files there are applied by the updater on every build and would add a second
realm to every installation. Run it by hand on the auth database, only for the capture, and delete realm 74 afterwards
(`DELETE FROM realmlist WHERE id = 74;`).

A realm only lists for and accepts clients of its exact `gamebuild`, and one worldserver serves one realm, so the test realm needs its
own worldserver: copy `worldserver.conf`, apply `doc/classic70532/worldserver_test70532.conf.example`, create the characters database named
there (empty; the updater fills it) and start `worldserver -c worldserver_test70532.conf`. bnetserver reads `build_info` only at
startup: restart it once after the update is applied.

How the 70532 client is pointed at realm 74 (the "super realm" address 70-1-74 in the launcher's `WTF/ForeverLauncher.wtf`) is not
verified.

## Capture plan: one 70532 login on port 1119

Run on the server PC with the 70532 client on the same or another PC. Nobody else online. One attempt per capture.

1. Logging. In `bnetserver.conf` set
   `Logger.session=1,Console Bnet`, `Logger.session.packets=1,Console Bnet`, `Appender.Bnet=2,1,0,Bnet.log,w`,
   then restart bnetserver. Handshake failures are logged as `<ip>:<port> SSL Handshake failed <reason>` on logger `session`.
   Keep `Bnet.log` and the console output of the attempt.
2. Packet capture (pick one, admin PowerShell):
   - pktmon (built into Windows 10/11):
     `pktmon filter remove`, `pktmon filter add -p 1119`, `pktmon filter add -p 8081`,
     `pktmon start --capture --pkt-size 0 -f cap70532.etl`, run the login attempt, `pktmon stop`,
     `pktmon etl2pcap cap70532.etl --out cap70532.pcapng`.
   - or Wireshark with capture filter `tcp port 1119 or tcp port 8081` on the loopback / LAN adapter (loopback needs Npcap's
     loopback adapter), save as pcapng.
3. Server-side TLS check from the same PC, before and after: `openssl s_client -connect <realm address>:1119 -servername <realm address>`
   and the same for port 8081; save both outputs (certificate chain, protocol, cipher).
4. Attempt order: (a) a 70450 client first if one is still around, as the working baseline capture; (b) then the 70532 client.
   One login each, with the account and realm that worked on 70450.
5. Client side: copy the client's newest `Errors`/`Logs` files from `_classic_beta_` (or `_forever_`) and `WTF/ForeverLauncher.wtf`.
6. Bring back: both pcapng files, `Bnet.log`, the console output, the openssl outputs, the client logs. In the capture we look for:
   whether a ClientHello arrives at all, which TLS version and server_name it offers, which alert (or TCP reset) ends the session, and
   which side closes first.
