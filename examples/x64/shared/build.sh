#!/bin/bash
# ============================================================
# Build script for DASM shared library example
# ============================================================
set -e
DEMI="../../../bin/demi-engine-debug"
TARGET="--assembly-target x86-elf64"

echo "=== Step 1: Build shared library ==="
$DEMI -A libgreet.asm $TARGET -o libgreet.so --shared
echo "Created: libgreet.so ($(file libgreet.so | cut -d: -f2))"

echo ""
echo "=== Step 2: Compile C test stub ==="
gcc -no-pie -o test_stub test_stub.c -L. -lgreet -Wl,-rpath,.
echo "Created: test_stub"

echo ""
echo "=== Step 3: Run ==="
./test_stub
