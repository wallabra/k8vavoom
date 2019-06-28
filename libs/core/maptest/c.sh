#!/bin/sh

odir=`pwd`
mdir=`dirname "$0"`
cd "$mdir"

rm -rf obj 2>/dev/null
mkdir obj

gcc -c -o obj/stats.o          -O2 -Wall -Wno-invalid-memory-model  ../mimalloc/stats.c
gcc -c -o obj/os.o             -O2 -Wall -Wno-invalid-memory-model  ../mimalloc/os.c
gcc -c -o obj/segment.o        -O2 -Wall -Wno-invalid-memory-model  ../mimalloc/segment.c
gcc -c -o obj/page.o           -O2 -Wall -Wno-invalid-memory-model  ../mimalloc/page.c
gcc -c -o obj/alloc.o          -O2 -Wall -Wno-invalid-memory-model  ../mimalloc/alloc.c
gcc -c -o obj/alloc-aligned.o  -O2 -Wall -Wno-invalid-memory-model  ../mimalloc/alloc-aligned.c
gcc -c -o obj/heap.o           -O2 -Wall -Wno-invalid-memory-model  ../mimalloc/heap.c
gcc -c -o obj/options.o        -O2 -Wall -Wno-invalid-memory-model  ../mimalloc/options.c
gcc -c -o obj/init.o           -O2 -Wall -Wno-invalid-memory-model  ../mimalloc/init.c

g++ -pthread -Wno-ignored-attributes -o map_test -Wall -O2 map_test.cpp ../zone.cpp obj/*.o

res=$?

cd "$odir"
exit $res
