#!/bin/sh
# Creates your own certificate authority (CA) and the bnetserver certificate for trinity.actual.battle.net,
# the name the Forever client logs in to. Run it once, on your own machine. Output in ./certs:
#
#   ca.key.pem        CA private key. Keep it secret and never copy it to the server: anyone who has it can
#                     issue certificates that your players' PCs trust.
#   ca.cert.pem       CA certificate. Every player imports it as a trusted root certificate.
#   server.key.pem    bnetserver private key (goes to the server).
#   server.chain.pem  bnetserver certificate + CA certificate (goes to the server).
#
# The CA is name-constrained to actual.battle.net, so even a leaked CA key can't impersonate other sites.
set -eu

OPENSSL="${OPENSSL:-openssl}"
dir="$(cd "$(dirname "$0")" && pwd)/certs"
mkdir -p "$dir"
cd "$dir"
if [ -e ca.key.pem ]; then
    echo "certs/ca.key.pem already exists. To start over, delete certs/*.pem (players must then import the new CA)." >&2
    exit 1
fi

umask 077
cat > ca.cnf <<'CNF'
[req]
distinguished_name = dn
prompt = no
[dn]
CN = Forever server private CA
[v3_ca]
basicConstraints = critical, CA:true, pathlen:0
keyUsage = critical, keyCertSign, cRLSign
subjectKeyIdentifier = hash
nameConstraints = critical, permitted;DNS:actual.battle.net
CNF
cat > server.ext <<'EXT'
basicConstraints = critical, CA:false
keyUsage = critical, digitalSignature, keyEncipherment
extendedKeyUsage = serverAuth
subjectAltName = DNS:trinity.actual.battle.net
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid
EXT

"$OPENSSL" req -x509 -newkey rsa:3072 -nodes -sha256 -days 3650 -config ca.cnf -extensions v3_ca \
    -keyout ca.key.pem -out ca.cert.pem
"$OPENSSL" req -newkey rsa:2048 -nodes -sha256 -subj "/CN=trinity.actual.battle.net" \
    -keyout server.key.pem -out server.csr
"$OPENSSL" x509 -req -sha256 -days 825 -in server.csr -CA ca.cert.pem -CAkey ca.key.pem -CAcreateserial \
    -extfile server.ext -out server.cert.pem
cat server.cert.pem ca.cert.pem > server.chain.pem
rm -f ca.cnf server.ext server.csr server.cert.pem ca.cert.srl
chmod 644 ca.cert.pem server.chain.pem

"$OPENSSL" verify -CAfile ca.cert.pem server.chain.pem
echo "Done. Give players certs/ca.cert.pem; keep certs/ca.key.pem to yourself."
