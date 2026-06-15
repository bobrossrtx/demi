#!/bin/bash
set -e
DEMI="../../../../bin/demi-engine-debug"
TARGET="--assembly-target x86-elf64"
echo ">>> DASM → DASM .so import <<<"

echo "Step 1: Build shared library"
$DEMI -A libimport.asm $TARGET -o libimport.so --shared
echo "  Created: libimport.so ($(file libimport.so | cut -d: -f2))"

echo "Step 2: Assemble main program"
$DEMI -A prog.asm $TARGET -ao prog.o
echo "  Created: prog.o"

echo "Step 3: Link against shared library"
gcc -no-pie -o prog prog.o -L. -l:libimport.so -Wl,-rpath,"$PWD"
echo "  Created: prog"

echo "Step 4: Run"
./prog
echo "  (prog exited cleanly — DASM→.so import works!)"
