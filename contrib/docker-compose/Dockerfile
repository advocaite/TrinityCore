# syntax=docker/dockerfile:1
#
# TrinityCore "forever" (WoW Classic beta 1.60) server: build and runtime image. The build context is the repository
# root, see compose.yaml next to this file and doc/Docker.md.

ARG BASE_IMAGE=ubuntu:24.04

FROM ${BASE_IMAGE} AS build
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      g++ make cmake ninja-build \
      libboost-dev libboost-filesystem-dev libboost-locale-dev libboost-program-options-dev \
      libboost-regex-dev libboost-thread-dev \
      libmysqlclient-dev mysql-client libssl-dev libreadline-dev libncurses-dev libbz2-dev zlib1g-dev \
 && rm -rf /var/lib/apt/lists/*

COPY . /src

# BUILD_JOBS: parallel compile jobs. Default: one per CPU, but at most one per 2.5 GB of RAM, because GCC needs over 2 GB
# for the largest files.
ARG BUILD_JOBS=
ARG BUILD_TYPE=Release
# 1 builds the spell test module (src/modules/SpellTest)
ARG BUILD_SPELL_TEST=0
# The build directory lives in a BuildKit cache, so a rebuild after git pull only recompiles what changed.
RUN --mount=type=cache,id=trinitycore-build,target=/build,sharing=locked <<EOF
set -eu
jobs="${BUILD_JOBS}"
if [ -z "$jobs" ]; then
  jobs=$(nproc)
  mem_jobs=$(awk '/MemTotal/ { print int($2 / 2621440) }' /proc/meminfo)
  if [ "$mem_jobs" -lt "$jobs" ]; then jobs=$mem_jobs; fi
  if [ "$jobs" -lt 1 ]; then jobs=1; fi
fi
echo "Building with $jobs parallel jobs"
# Ninja rebuilds what is newer than its output, and COPY keeps the host's modification times: a file edited while the
# previous build ran is older than the object built from its old content. Touch every file whose content changed.
cd /src
find src dep cmake CMakeLists.txt -type f -print0 | sort -z | xargs -0 sha1sum > /tmp/sources.sha1
if [ -f /build/sources.sha1 ]; then
  diff /build/sources.sha1 /tmp/sources.sha1 | sed -n 's/^> [0-9a-f]*  //p' | while IFS= read -r file; do touch "$file"; done
fi
cmake -S /src -B /build -G Ninja \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
  -DCMAKE_INSTALL_PREFIX=/opt/trinitycore \
  -DSERVERS=1 -DTOOLS=1 -DSCRIPTS=static \
  -DWITHOUT_GIT=1 -DBUILD_TESTING=0 \
  -DBUILD_SPELL_TEST="${BUILD_SPELL_TEST}"
cmake --build /build --parallel "$jobs"
cmake --install /build
cp /tmp/sources.sha1 /build/sources.sha1
EOF


FROM ${BASE_IMAGE}
ARG DEBIAN_FRONTEND=noninteractive
# mysql-client: the database updater imports SQL files with the mysql command line client.
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      libboost-filesystem1.83.0 libboost-locale1.83.0 libboost-program-options1.83.0 libboost-regex1.83.0 \
      libmysqlclient21 mysql-client libssl3t64 libreadline8t64 libbz2-1.0 zlib1g \
 && rm -rf /var/lib/apt/lists/*

COPY --from=build /opt/trinitycore /opt/trinitycore
# The database updater reads sql/base, sql/updates and sql/custom at runtime. sql/old (4 GB of archived updates) is not in
# the build context.
COPY --from=build /src/sql /opt/trinitycore/sql

# Fail here rather than at runtime if a shared library is missing.
RUN ! ldd /opt/trinitycore/bin/* 2>/dev/null | grep "not found"

# Fixed IDs, so files mounted from the host (the bnetserver key) can be given to this user.
RUN groupadd --system --gid 10001 trinity \
 && useradd --system --uid 10001 --gid trinity --no-create-home --home-dir /opt/trinitycore trinity \
 && cp /opt/trinitycore/etc/worldserver.conf.dist /opt/trinitycore/etc/worldserver.conf \
 && cp /opt/trinitycore/etc/bnetserver.conf.dist /opt/trinitycore/etc/bnetserver.conf \
 && mkdir -p /opt/trinitycore/data /opt/trinitycore/logs /opt/trinitycore/tdb /opt/trinitycore/certs \
 && chown trinity:trinity /opt/trinitycore/logs

# Paths of this image. Everything else is configured with TC_* variables in compose.yaml.
ENV PATH=/opt/trinitycore/bin:$PATH \
    TC_SOURCE_DIRECTORY=/opt/trinitycore \
    TC_DATA_DIR=/opt/trinitycore/data \
    TC_LOGS_DIR=/opt/trinitycore/logs
WORKDIR /opt/trinitycore
USER trinity
CMD ["worldserver"]
