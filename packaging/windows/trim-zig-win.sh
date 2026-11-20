#!/bin/sh
# SPDX-FileCopyrightText: 2026 jerbanpls
# SPDX-License-Identifier: GPL-3.0-or-later
#
# trim-zig-win.sh ZIGDIR: shrink an unpacked zig release for Windows (the
# zip) for the package. It only compiles and links C for x86_64-windows-gnu,
# so what stays is the compiler's own library (std, compiler_rt, the clang
# headers) and mingw-w64; the libc sources and headers of every other target,
# the C++ runtimes, sanitizers and docs go. Like packaging/trim-zig.sh.
set -eu
z=$1
rm -rf "$z/doc" "$z/lib/docs" "$z/lib/libcxx" "$z/lib/libcxxabi" "$z/lib/libunwind" \
    "$z/lib/libtsan" "$z/lib/build-web" "$z/lib/init" "$z/lib/fuzzer.zig"
libc="$z/lib/libc"
rm -rf "$libc/darwin" "$libc/freebsd" "$libc/netbsd" "$libc/wasi" "$libc/glibc" "$libc/musl"
for d in "$libc"/include/*; do
    case "${d##*/}" in
        any-windows-any) ;;
        *) rm -rf "$d" ;;
    esac
done
