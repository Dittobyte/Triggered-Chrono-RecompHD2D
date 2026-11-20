#!/bin/sh
# SPDX-FileCopyrightText: 2026 jerbanpls
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Tests of the Windows package under Wine (EXPERIMENTAL).
#
#   packaging/windows/test-wine.sh OUT ctest
#   packaging/windows/test-wine.sh OUT module      (not implemented yet)
#   packaging/windows/test-wine.sh OUT rom ROMFILE (not implemented yet)
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
ctest) ;;
module|rom) echo "test-wine.sh: mode $mode is not implemented yet" >&2; exit 2 ;;
*) echo "test-wine.sh: unknown mode $mode" >&2; exit 2 ;;
esac
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
