#!/bin/bash
# Build DASM shared library — math library example
set -e
DEMI="../../../../bin/demi-engine-debug"
TARGET="--assembly-target x86-elf64"
echo ">>> Building math shared library <<<"
$DEMI -A libmath.asm $TARGET -o libmath.so --shared
gcc -no-pie -o test_stub test_stub.c -L. -l:libmath.so -Wl,-rpath,.
echo "=== Run ==="
./test_stub
