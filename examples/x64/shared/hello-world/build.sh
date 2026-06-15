#!/bin/bash
# Build DASM shared library — hello-world example
set -e
DEMI="../../../../bin/demi-engine-debug"
TARGET="--assembly-target x86-elf64"
echo ">>> Building hello-world shared library <<<"
$DEMI -A libhello.asm $TARGET -o libhello.so --shared
gcc -no-pie -o test_stub test_stub.c -L. -l:libhello.so -Wl,-rpath,.
echo "=== Run ==="
./test_stub
