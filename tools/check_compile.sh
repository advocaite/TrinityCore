#!/bin/bash
# Compile check without a full build: syntax-only (-fsyntax-only) over the .cpp files a range of commits changed,
# using the real compile flags from a cached CMake configure (SCRIPTS=none, bots on); module files get PCH.h
# force-included like the real build.
#   tools/check_compile.sh <base> [<head>]     head defaults to HEAD and must be the checked-out commit
# Also checks every module .cpp that textually includes a changed module header.
# Env: CC_BUILD (cache dir, default ~/.cache/wf-cc-build), CC_JOBS (default nproc), CC_ALL=1 (every module .cpp).
# Exit 0 = all clean, 1 = a file failed, 2 = setup problem.
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BASE="${1:-}"; HEAD="${2:-HEAD}"
[ -n "$BASE" ] || [ "${CC_ALL:-}" = 1 ] || { echo "usage: $0 <base> [<head>]" >&2; exit 2; }
BUILD="${CC_BUILD:-$HOME/.cache/wf-cc-build}"
JOBS="${CC_JOBS:-$(nproc)}"
MOD=src/modules/PlayerbotV2
cd "$ROOT" || exit 2
[ "$(git rev-parse HEAD)" = "$(git rev-parse "$HEAD")" ] || { echo "check out $HEAD first (the compile reads the working tree)" >&2; exit 2; }

# Dependencies (headers only are needed; Debian/Ubuntu).
if [ ! -d /usr/include/boost ] || [ ! -f /usr/include/mysql/mysql.h ]; then
    echo "[setup] installing build headers"
    (apt-get update -qq && apt-get install -y -qq cmake ninja-build g++ libboost-all-dev libmysqlclient-dev \
        libssl-dev libreadline-dev zlib1g-dev libbz2-dev) >/dev/null 2>&1 || { echo "dependency install failed" >&2; exit 2; }
fi

# Configure once; again whenever a CMakeLists changed or a file was added/removed (sources are globbed).
STAMP="$BUILD/.cc-stamp"
SIG="$(git ls-files "$MOD" 'CMakeLists.txt' 'src/**/CMakeLists.txt' 'cmake' | grep -E '(CMakeLists\.txt|\.cmake|\.cpp|\.h)$' | sha1sum | cut -d' ' -f1)"
if [ ! -f "$BUILD/compile_commands.json" ] || [ "$(cat "$STAMP" 2>/dev/null)" != "$ROOT $SIG" ]; then
    echo "[setup] cmake configure in $BUILD"
    mkdir -p "$BUILD"
    cmake -S "$ROOT" -B "$BUILD" -G Ninja -DSCRIPTS=none -DTOOLS=0 -DWITH_WARNINGS=0 -DUSE_COREPCH=0 -DUSE_SCRIPTPCH=0 \
        -DBUILD_PLAYERBOT_V2=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON >"$BUILD/configure.log" 2>&1 \
        || { tail -20 "$BUILD/configure.log" >&2; echo "configure failed" >&2; exit 2; }
    echo "$ROOT $SIG" >"$STAMP"
fi

# Files to check.
LIST="$(mktemp)"; trap 'rm -f "$LIST" "$LIST.out"' EXIT
if [ "${CC_ALL:-}" = 1 ]; then
    git ls-files "$MOD" | grep -E '\.cpp$' >"$LIST"
else
    CHANGED="$(git diff --name-only --diff-filter=ACMR "$BASE" "$HEAD")"
    echo "$CHANGED" | grep -E '\.cpp$' >"$LIST"
    for h in $(echo "$CHANGED" | grep -E "^$MOD/.*\.(h|hpp)$"); do
        git grep -l -E "#include \"([^\"]*/)?$(basename "$h")\"" -- "$MOD" | grep -E '\.cpp$' >>"$LIST"
    done
    sort -u "$LIST" -o "$LIST"
fi
N=$(wc -l <"$LIST")
[ "$N" -gt 0 ] || { echo "no .cpp files to check"; exit 0; }
echo "[check] $N file(s), $JOBS jobs"

python3 -I - "$BUILD/compile_commands.json" "$ROOT" "$LIST" >"$LIST.out" <<'PY' || exit 2
import json, os, sys, shlex
db = {os.path.realpath(e['file']): e for e in json.load(open(sys.argv[1]))}
root = sys.argv[2]
for rel in open(sys.argv[3]).read().split():
    e = db.get(os.path.realpath(os.path.join(root, rel)))
    if not e:
        print("# no compile command (not part of the build): " + rel, file=sys.stderr); continue
    a = shlex.split(e['command']); out = []; i = 0
    while i < len(a):
        if a[i] in ('-o', '-MF', '-MT', '-MQ'): i += 2; continue
        if a[i] in ('-MD', '-MMD', '-c'): i += 1; continue
        if a[i] == a[-1] and a[i].endswith('.cpp'): i += 1; continue
        out.append(a[i]); i += 1
    # the real build force-includes the module PCH.h into every module file; do the same
    if rel.startswith('src/modules/PlayerbotV2/'): out += ['-include', os.path.join(root, 'src/modules/PlayerbotV2/PCH.h')]
    out += ['-fsyntax-only', '-w', e['file']]
    print(rel + '\t' + e['directory'] + '\t' + ' '.join(shlex.quote(x) for x in out))
PY

START=$(date +%s)
cut -f1-3 "$LIST.out" | xargs -d '\n' -P "$JOBS" -I{} bash -c '
    IFS=$'"'"'\t'"'"' read -r rel dir cmd <<<"$1"
    if (cd "$dir" && eval "$cmd") >"'"$LIST"'.$$.log" 2>&1; then echo "ok   $rel"
    else echo "FAIL $rel"; grep -E "error|Error" "'"$LIST"'.$$.log" | head -8 | sed "s/^/     /"; fi
    rm -f "'"$LIST"'.$$.log"' _ {} | tee "$LIST.res"
FAILS=$(grep -c '^FAIL' "$LIST.res"); rm -f "$LIST.res"
echo "[check] $((N - FAILS)) ok, $FAILS failed, $(( $(date +%s) - START )) s"
[ "$FAILS" -eq 0 ]
