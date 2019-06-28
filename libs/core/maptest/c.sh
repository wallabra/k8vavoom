#!/bin/sh

odir=`pwd`
mdir=`dirname "$0"`
cd "$mdir"

rm -rf obj 2>/dev/null
mkdir obj

g++ -pthread -Wno-ignored-attributes -o map_test -Wall -O2 map_test.cpp ../zone.cpp ../mimalloc/static.c

res=$?

cd "$odir"
exit $res
