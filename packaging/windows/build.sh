#!/bin/sh
# SPDX-FileCopyrightText: 2026 jerbanpls
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Cross-build the engine for 64-bit Windows with zig (EXPERIMENTAL package).
#
#   packaging/windows/build.sh [OUT]
#
# OUT (default: build-win64/ in the repo, git-ignored) gets downloads/, the
# CMake build, stage/ (the unpacked package) and CT_Recomp-win64.zip.
# Runs on Linux; without cmake on PATH it re-runs itself in the `ct-build`
# distrobox. Later leaves add Python, zig for Windows, tools and a launcher.
set -eu

repo=$(cd "$(dirname "$0")/../.." && pwd)
mkdir -p "${1:-$repo/build-win64}"
out=$(cd "${1:-$repo/build-win64}" && pwd)

if ! command -v cmake >/dev/null 2>&1; then
    command -v distrobox >/dev/null 2>&1 || { echo "build.sh: need cmake (or distrobox with ct-build)" >&2; exit 1; }
    exec distrobox enter ct-build -- sh "$repo/packaging/windows/build.sh" "$out"
fi

# ---- pinned downloads: URL, SHA-256 ----
ZIG_URL=https://ziglang.org/download/0.15.2/zig-x86_64-linux-0.15.2.tar.xz
ZIG_SHA=02aa270f183da276e5b5920b1dac44a63f1a49e55050ebde3aecc9eb82f93239
SDL_URL=https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-devel-2.32.10-mingw.tar.gz
SDL_SHA=83a5d74012311edc3c0d40ea6faecbe57ad692aa033fa5dc273cc937e3938ff2

dl="$out/downloads"
mkdir -p "$dl"
fetch() {   # fetch NAME URL SHA256: download once, verify every time
    f="$dl/$1"
    if [ ! -f "$f" ]; then
        curl -fL --retry 3 -o "$f.part" "$2"
        mv "$f.part" "$f"
    fi
    echo "$3  $f" | sha256sum -c - >/dev/null || {
        echo "build.sh: SHA-256 mismatch for $1" >&2
        rm -f "$f"
        exit 1
    }
}
fetch zig-linux.tar.xz "$ZIG_URL" "$ZIG_SHA"
fetch SDL2-devel-mingw.tar.gz "$SDL_URL" "$SDL_SHA"

tools="$out/tools"
if [ ! -x "$tools/zig/zig" ]; then
    rm -rf "$tools/zig"; mkdir -p "$tools/zig"
    tar -xJf "$dl/zig-linux.tar.xz" -C "$tools/zig" --strip-components=1
fi
if [ ! -d "$tools/SDL2-2.32.10" ]; then
    tar -xzf "$dl/SDL2-devel-mingw.tar.gz" -C "$tools"
fi
sdl="$tools/SDL2-2.32.10/x86_64-w64-mingw32"
export CT_ZIG="$tools/zig/zig"
# zig's cache inside OUT: nothing outside the build tree is written.
export ZIG_GLOBAL_CACHE_DIR="$out/zig-cache" ZIG_LOCAL_CACHE_DIR="$out/zig-cache"

bld="$out/build"
cmake -S "$repo" -B "$bld" -G "${CMAKE_GENERATOR:-Unix Makefiles}" \
    -DCMAKE_TOOLCHAIN_FILE="$repo/cmake/zig-windows.cmake" \
    -DCMAKE_BUILD_TYPE=Release -DCT_ENGINE_ONLY=ON \
    -DCMAKE_PREFIX_PATH="$sdl" -DCMAKE_INSTALL_PREFIX="$out/stage"
build() { cmake --build "$bld" -j "${JOBS:-$(nproc)}" --target ct_sdl_engine ct_boot_engine; }
build
# zig's linker exports nothing from an exe by itself: list the engine's
# global symbols (the whole-archive libraries) in a .def file and link again;
# CMake's --out-implib then yields libct_*_engine.dll.a too.
def="$out/ct_engine.def"
{
    echo EXPORTS
    for l in libct_sched.a libct_interp.a libct_runtime.a; do
        nm -g --defined-only "$bld/$l"
    done | awk '$2 ~ /^[TDBRC]$/ && $3 !~ /^[._]/ && $3 != "main" {
        print "    " $3 ($2 == "T" ? "" : " DATA") }' | sort -u
} > "$def"
echo "exported symbols: $(($(wc -l < "$def") - 1))"
cmake -S "$repo" -B "$bld" -DCT_EXE_DEF="$def"
rm -f "$bld/ct_sdl_engine.exe" "$bld/ct_boot_engine.exe"
build

# The Windows package layout is flat (not the Linux install tree): programs,
# SDL2.dll and the import libraries next to each other, licences in licenses/.
rm -rf "$out/stage"; mkdir -p "$out/stage/licenses"
cp "$bld/ct_sdl_engine.exe" "$bld/ct_boot_engine.exe" "$sdl/bin/SDL2.dll" "$out/stage/"
cp "$bld/libct_sdl_engine.dll.a" "$bld/libct_boot_engine.dll.a" "$out/stage/"
cp -r "$repo/LICENSES" "$repo/THIRD_PARTY.md" "$out/stage/licenses/"

rm -f "$out/CT_Recomp-win64.zip"
python3 - "$out/stage" "$out/CT_Recomp-win64.zip" <<'PY'
import os, sys, zipfile
src, dst = sys.argv[1:3]
with zipfile.ZipFile(dst, "w", zipfile.ZIP_DEFLATED) as z:
    for root, _, files in os.walk(src):
        for f in sorted(files):
            p = os.path.join(root, f)
            z.write(p, os.path.join("CT_Recomp", os.path.relpath(p, src)))
PY
ls -l "$out/stage"
echo WIN-BUILD-OK
