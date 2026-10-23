#!/bin/sh
# Builds build/map-warp/ (the installable package) and build/map-warp.zip.
# Needs a clang with the PowerPC target and ld.lld (macOS: brew install llvm lld), plus the SDK
# headers shipped with the port: SDK=/path/to/WindWakerHD/sdk/guest/include
set -eu
cd "$(dirname "$0")"

: "${SDK:?set SDK to the sdk/guest/include folder of the port}"
CLANG="${CLANG:-clang}"
LLD="${LLD:-ld.lld}"
if command -v brew >/dev/null 2>&1; then
    [ "$CLANG" = clang ] && [ -x "$(brew --prefix llvm 2>/dev/null)/bin/clang" ] && CLANG="$(brew --prefix llvm)/bin/clang"
    [ "$LLD" = ld.lld ] && [ -x "$(brew --prefix lld 2>/dev/null)/bin/ld.lld" ] && LLD="$(brew --prefix lld)/bin/ld.lld"
fi

OUT=build/map-warp
rm -rf build
mkdir -p "$OUT"
"$CLANG" --target=powerpc-unknown-eabi -mcpu=750 -O2 -ffreestanding -fno-builtin -nostdlib \
    -fno-jump-tables -ffunction-sections -fdata-sections -Wall -I"$SDK" \
    -c src/mod.c -o build/mod.o
"$LLD" -m elf32ppc -r build/mod.o -o "$OUT/mod.elf"
cp manifest.json README.md LICENSE "$OUT/"
(cd build && zip -qr map-warp.zip map-warp)
echo "built $OUT and build/map-warp.zip"
