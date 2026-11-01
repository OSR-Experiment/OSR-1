#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Obsimo (OSR Records)
# The browser version of OSR-1 (the host simulator in WebAssembly) with Zig instead of Emscripten:
#   pip install ziglang && tools/build_web.sh        ->  build/emu/app.wasm (+ fm1-base's page, worklet.js)
# Needs the host build's generated headers first: python3 fm1-base/tools/build.py . --host
set -e
cd "$(dirname "$0")/.."
ZIG="${ZIG:-python3 -m ziglang}"
S=fm1-base
mkdir -p build/emu
$ZIG cc -target wasm32-wasi -mexec-model=reactor -O2 -ffp-contract=off -std=gnu99 -w -DPLAT_HOST -DPLAT_WEB \
    -I$S/host -I$S/device -I$S/include -I$S/lib -I. -Ibuild/gen -Idsp \
    -Wl,--export-dynamic -Wl,-z,stack-size=1048576 -o build/emu/app.wasm $S/web/emu/emu_web.c dsp/engine.c
# the page: fm1-base's (the FM-1 drawn as it is), with the app's name
NAME="$(sed -n 's/^name *= *"\(.*\)"/\1/p' fm1.toml | head -1)"
sed -e "s/{{NAME}}/$NAME/g" -e "s/{{ID}}/blop/g" $S/web/emu/index.html > build/emu/index.html
cp $S/web/emu/worklet.js build/emu/
echo "web: build/emu/ (serve it over http: python3 -m http.server -d build/emu)"
