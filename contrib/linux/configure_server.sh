#!/bin/sh
# Creates bnetserver.conf and worldserver.conf for a public Forever server (one realm, Normal ruleset)
# from the installed .conf.dist files. Only the keys a public Classic server needs are changed.
#
# Usage: configure_server.sh <install prefix> <public ip> <mysql password>
#   e.g. configure_server.sh /root/server 203.0.113.7 "$(cat /root/.forever_db_pass)"
set -e

PREFIX="$1"
PUBLIC_IP="$2"
DB_PASS="$3"
if [ -z "$PREFIX" ] || [ -z "$PUBLIC_IP" ] || [ -z "$DB_PASS" ]; then
    echo "usage: $0 <install prefix> <public ip> <mysql password>" >&2
    exit 1
fi

ETC="$PREFIX/etc"
BNET="$ETC/bnetserver.conf"
WORLD="$ETC/worldserver.conf"

# setkey <file> <key> <value>: replaces "key = ..." or appends it when the .dist has no such line
setkey() {
    if grep -qE "^$2[[:space:]]*=" "$1"; then
        sed -i -E "s|^$2[[:space:]]*=.*|$2 = $3|" "$1"
    else
        printf '\n%s = %s\n' "$2" "$3" >> "$1"
    fi
}

cp "$ETC/bnetserver.conf.dist" "$BNET"
cp "$ETC/worldserver.conf.dist" "$WORLD"
mkdir -p "$PREFIX/logs"

# bnetserver: listen on all interfaces; the client is always told the hostname (TLS certificate name).
# Both LoginREST names must resolve on this machine too (see /etc/hosts in the guide).
setkey "$BNET" LoginDatabaseInfo "\"127.0.0.1;3306;trinity;$DB_PASS;auth\""
setkey "$BNET" BindIP '"0.0.0.0"'
setkey "$BNET" LoginREST.ExternalAddress trinity.actual.battle.net
setkey "$BNET" LoginREST.LocalAddress trinity.actual.battle.net
setkey "$BNET" CertificatesFile '"./forever.cert.pem"'
setkey "$BNET" PrivateKeyFile '"./forever.key.pem"'
setkey "$BNET" LogsDir "\"$PREFIX/logs\""
setkey "$BNET" Realm.CfgContentSetID 137
setkey "$BNET" Realm.SuperDistrictID 2

# worldserver: Classic client requirements (README), public bind, updater on
for db in Login:auth World:world Character:characters Hotfix:hotfixes; do
    setkey "$WORLD" "${db%%:*}DatabaseInfo" "\"127.0.0.1;3306;trinity;$DB_PASS;${db##*:}\""
done
setkey "$WORLD" RealmID 70
setkey "$WORLD" BindIP '"0.0.0.0"'
setkey "$WORLD" DataDir "\"$PREFIX/data\""
setkey "$WORLD" LogsDir "\"$PREFIX/logs\""
setkey "$WORLD" Network.SkipBuildAuthKeyCheck 1
setkey "$WORLD" Network.EnterEncryptedModeRegionGroup 8
setkey "$WORLD" Realm.CfgContentSetID 137
setkey "$WORLD" Realm.SuperDistrictID 2
setkey "$WORLD" Updates.EnableDatabases 15
setkey "$WORLD" Updates.AutoSetup 1
setkey "$WORLD" mmap.enablePathFinding 0
# DB warnings (about a million lines on every start: retail rows the Classic data does not have) go to DBErrors.log only
setkey "$WORLD" Logger.sql.sql 5,DBErrors
setkey "$WORLD" Ra.Enable 0
setkey "$WORLD" SOAP.Enabled 0
# same gameplay settings as the repack
setkey "$WORLD" Classic.GuildFinder 1
setkey "$WORLD" Classic.Shop.Enabled 1
setkey "$WORLD" AccountInstancesPerHour 5
setkey "$WORLD" PlayerSave.Stats.MinLevel 1

chmod 600 "$BNET" "$WORLD"
echo "written: $BNET $WORLD"
echo "realmlist must point at $PUBLIC_IP (see guide), hosts entry: $PUBLIC_IP trinity.actual.battle.net"
