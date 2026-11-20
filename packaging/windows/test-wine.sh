#!/bin/sh
# SPDX-FileCopyrightText: 2026 jerbanpls
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Tests of the Windows package under Wine (EXPERIMENTAL).
#
#   packaging/windows/test-wine.sh OUT ctest
#   packaging/windows/test-wine.sh OUT module
#   packaging/windows/test-wine.sh OUT rom ROMFILE (not implemented yet)
#
# module: in a throwaway Wine prefix, PATH holding only the package's Python
# and zig, builds packaging/windows/synth (the smallest valid game module, no
# game data) with tools/build_module.py --synth and loads it with
# ct_boot_engine.exe --module; prints WIN-MODULE-OK when the engine reports the
# module's two functions.
#
# ctest: cross-builds every test program next to the engine in OUT/build
# (build.sh must have run), then runs the ROM-free set (ctest -LE rom) as
# packaging/windows/ctest-wine.py describes, and prints a ctest summary.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../.." && pwd)
[ $# -ge 2 ] || { echo "usage: test-wine.sh OUT ctest|module|rom ROMFILE" >&2; exit 2; }
out=$(cd "$1" && pwd)
mode=$2
case $mode in
ctest|module) ;;
rom) echo "test-wine.sh: mode $mode is not implemented yet" >&2; exit 2 ;;
*) echo "test-wine.sh: unknown mode $mode" >&2; exit 2 ;;
esac
if [ "$mode" = module ]; then
    [ -f "$out/stage/python/python.exe" ] && [ -f "$out/stage/zig/zig.exe" ] ||
        { echo "test-wine.sh: run build.sh $out first (the package has no python/zig)" >&2; exit 1; }
    rc=0
    # Wine cannot answer the NtQueryObject question zig.exe asks about its own
    # path: the test runs a copy of the package's zig.exe with a shim
    # (packaging/windows/wine-zig/, Wine only; the package's own file is untouched).
    mkdir -p "$out/wzig"
    python3 -I "$here/wine-zig/patch-zig.py" "$out/stage/zig/zig.exe" "$out/wzig"
    distrobox enter ct-build -- sh -c 'export ZIG_GLOBAL_CACHE_DIR="$1/zig-cache"; "$1/tools/zig/zig" cc -target x86_64-windows-gnu -shared -O2 -o "$1/wzig/ntdlx.dll" "$2/wine-zig/shim.c" "$1/wzig/ntdlx.def"' \
        sh "$out" "$here" >"$out/module-test.log" 2>&1 || { cat "$out/module-test.log"; echo "test-wine.sh: building the Wine zig shim failed" >&2; exit 1; }
    rm -rf "$out/stage/synth"; cp -r "$here/synth" "$out/stage/synth"
    trap 'rm -rf "$out/stage/synth"' EXIT
    # zig builds the mingw-w64 runtime for the DLL on first use: about half an hour
    # under Wine. Its cache is kept in OUT/wine-zig-cache (CT_WINE_FRESH=1 starts
    # over), so only the first run is slow. The Wine prefix itself is new every run.
    [ -z "${CT_WINE_FRESH:-}" ] || rm -rf "$out/wine-zig-cache"
    mkdir -p "$out/wine-zig-cache"
    CT_WINE_PODMAN_ARGS="-v $out/wzig:/home/wine/prefix/drive_c/wzig:ro -v $out/wine-zig-cache:/home/wine/prefix/drive_c/zcache" "$here/wine.sh" "$out" -- wine cmd /c 'mkdir C:\work & C:\pkg\synth\test-module.bat' >"$out/module-test.log" 2>&1 || rc=$?
    cat "$out/module-test.log"
    if [ $rc -eq 0 ] && grep -q 'ct_boot: game module .*ct_synth.dll: 2 functions' "$out/module-test.log"; then
        echo WIN-MODULE-OK
        exit 0
    fi
    echo "test-wine.sh: the module did not build and load (exit $rc)" >&2
    exit 1
fi
[ -f "$out/build/CMakeCache.txt" ] || { echo "test-wine.sh: run build.sh $out first" >&2; exit 1; }

# the tests build with the same toolchain build.sh used
box() { distrobox enter ct-build -- "$@"; }
box sh -c 'cd "$1" && export CT_ZIG="$1/tools/zig/zig" ZIG_GLOBAL_CACHE_DIR="$1/zig-cache" ZIG_LOCAL_CACHE_DIR="$1/zig-cache" &&
    cmake --build build -j "$(nproc)"' sh "$out" >"$out/test-build.log" 2>&1 || {
    tail -30 "$out/test-build.log"; echo "test-wine.sh: building the tests failed" >&2; exit 1; }

cp "$out/stage/SDL2.dll" "$out/build/"   # the test programs load it from their own folder

# cmd.exe stand-in (system()/popen() run POSIX shell lines in these tests)
mkdir -p "$out/testbin"
box sh -c 'export ZIG_GLOBAL_CACHE_DIR="$1/zig-cache"; "$1/tools/zig/zig" cc -target x86_64-windows-gnu -O2 -o "$1/testbin/cmd.exe" "$2"' \
    sh "$out" "$here/shim-cmd.c" >>"$out/test-build.log" 2>&1 || { echo "test-wine.sh: shim-cmd failed" >&2; exit 1; }

# how many ROM-free tests the Linux engine-only build has (configure only)
box sh -c 'cmake -S "$1" -B "$2/linux-eo" -DCT_ENGINE_ONLY=ON -DCMAKE_BUILD_TYPE=Release >/dev/null 2>&1 &&
    cd "$2/linux-eo" && ctest -N -LE rom | sed -n "s/^Total Tests: //p"' sh "$repo" "$out" >"$out/linux-count.txt"
n=$(cat "$out/linux-count.txt")
[ -n "$n" ] || { echo "test-wine.sh: could not count the Linux tests" >&2; exit 1; }

exec python3 -I "$here/ctest-wine.py" "$out" "$repo" "$n"
