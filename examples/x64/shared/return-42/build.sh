#!/bin/bash
# Build DASM shared library — return-42 example
set -e
DEMI="../../../../bin/demi-engine-debug"
TARGET="--assembly-target x86-elf64"
echo ">>> Building return-42 shared library <<<"
$DEMI -A libreturn42.asm $TARGET -o libreturn42.so --shared
gcc -no-pie -o test_stub test_stub.c -L. -l:libreturn42.so -Wl,-rpath,.
echo "=== Run ==="
./test_stub
