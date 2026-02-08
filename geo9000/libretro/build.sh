#!/bin/sh
set -e 
make clean && make
cp *_libretro.dylib ../../e9k-debugger/system/geo9000.dylib
echo "Copied to geo9000.dylib ../../e9k-debugger/system"
