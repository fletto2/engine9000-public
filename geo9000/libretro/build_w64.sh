#!/bin/sh
set -e
make clean
CC=x86_64-w64-mingw32-gcc platform=win64 make
cp *_libretro.dll ../../e9k-debugger/system/geo9000.dll
echo "copied to geo9000.dll ../../e9k-debugger/system"
