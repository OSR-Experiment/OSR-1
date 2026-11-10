#!/bin/sh
# Build Blop's firmware (build/blop.fwsc) on a Mac, in a Linux container (Docker Desktop must be running).
# The same machine as the GitHub workflow: Ubuntu 22.04, JieLi's Linux toolchain and the AC79 SDK from ~/.jieli.
#   tools/build_mac.sh            the firmware, after the simulator's tests
#   tools/build_mac.sh v3-name    the same, then kept in versions/v3-name/ (the .fwsc and the source that made it)
# First time: fm1-base/tools/get_toolchain.sh ~/.jieli, and the SDK in ~/.jieli/ac79-sdk (see .github/workflows).
set -e
cd "$(dirname "$0")/.."
command -v docker >/dev/null || { echo "Docker not found: install Docker Desktop and open it"; exit 1; }
docker info >/dev/null 2>&1 || { echo "Docker is not running: open Docker Desktop and wait for it to start"; exit 1; }
[ -d "$HOME/.jieli/toolchain/" ] || { echo "no toolchain in ~/.jieli/toolchain"; exit 1; }
[ -f "$HOME/.jieli/ac79-sdk/cpu/wl82/tools/uboot.boot" ] || { echo "no SDK in ~/.jieli/ac79-sdk"; exit 1; }
IMG=blop-build:22.04
if ! docker image inspect "$IMG" >/dev/null 2>&1; then
    echo "first run: making the build image (a few minutes)"
    docker build --platform linux/amd64 -t "$IMG" - <<'EOF'
FROM ubuntu:22.04
RUN apt-get update -q && DEBIAN_FRONTEND=noninteractive apt-get install -y -q \
    python3 python3-pip build-essential libtinfo5 libncurses5 xz-utils libraqm0 git \
    && pip3 install -q Pillow fonttools && rm -rf /var/lib/apt/lists/*
EOF
fi
TC="$(readlink "$HOME/.jieli/toolchain")"
docker run --rm --platform linux/amd64 \
    -v "$PWD":/app -v "$HOME/.jieli":/jieli -w /app \
    -e JIELI_TOOLCHAIN="/jieli/$TC" -e AC79_SDK=/jieli/ac79-sdk "$IMG" sh -c '
        set -e
        rm -rf build/host
        # the tests check the standard defaults: a player'"'"'s my_setup.h waits aside meanwhile (back for the firmware)
        if [ -f my_setup.h ]; then mv my_setup.h .my_setup.h.aside; trap "mv .my_setup.h.aside my_setup.h" EXIT; fi
        python3 fm1-base/tools/build.py . --host
        build/host/blop_host tests/basic.txt build/host > build/host/basic.log
        build/host/blop_host tests/midi_in.txt build/host > build/host/midi_in.log
        echo "simulator tests: ok"
        if [ -f .my_setup.h.aside ]; then mv .my_setup.h.aside my_setup.h; trap - EXIT; echo "my_setup.h: in"; fi
        python3 fm1-base/tools/build.py .'
ls -la build/*.fwsc
if [ -n "$1" ]; then
    V="versions/$1"
    [ -e "$V" ] && { echo "$V already exists: pick a new name (nothing kept)"; exit 1; }
    mkdir -p "$V"
    cp build/blop.fwsc "$V/blop-${1%%-*}.fwsc"
    cp app.c tests/midi_in.txt "$V/"
    [ -f my_setup.h ] && cp my_setup.h "$V/"
    echo "kept: $V/blop-${1%%-*}.fwsc  (add a line about it to versions/README.md)"
fi
