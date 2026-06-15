#!/bin/bash
set -e
DEMI="../../../../bin/demi-engine-debug"
TARGET="--assembly-target x86-elf64"
echo ">>> Building DASM import example <<<"
$DEMI -A import-test.asm $TARGET -ao import-test.o

cat > test_driver.c << 'EOF'
#include <stdio.h>
extern int greet(void);
int main() { printf("greet() from DASM .so returned %d\n", greet()); return 0; }
EOF
gcc -no-pie -o test_driver test_driver.c -L../return-42 -l:libreturn42.so -Wl,-rpath,"$PWD/../return-42"
echo "=== C driver ==="
./test_driver

gcc -no-pie -o import-test import-test.o -L../return-42 -l:libreturn42.so -Wl,-rpath,"$PWD/../return-42"
echo "=== DASM import ==="
./import-test
echo "(import-test exited cleanly — greet() was called from DASM .so)"
