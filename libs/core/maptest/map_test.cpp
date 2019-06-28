//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2018-2019 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, either version 3 of the License, or
//**  (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**  You should have received a copy of the GNU General Public License
//**  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//**
//**************************************************************************
#define EXCESSIVE_CHECKS
#define EXCESSIVE_CHECKS_ITERATOR
#define EXCESSIVE_COMPACT

#define CORE_MAP_TEST

typedef unsigned int vuint32;


#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


static vuint32 GetTypeHash (int a) {
  vuint32 res = (vuint32)a;
  res -= (res<<6);
  res = res^(res>>17);
  res -= (res<<9);
  res = res^(res<<4);
  res -= (res<<3);
  res = res^(res<<10);
  res = res^(res>>15);
  return res;
}


static void fatal (const char *msg) {
  fprintf(stderr, "FATAL: %s\n", msg);
  abort();
}


void Sys_Error (const char *error, ...) {
  va_list argptr;
  static char buf[16384]; //WARNING! not thread-safe!

  va_start(argptr,error);
  vsnprintf(buf, sizeof(buf), error, argptr);
  va_end(argptr);

  fatal(buf);
}


#include "../common.h"
#include "../zone.h"
#include "../map.h"

//int zone_malloc_call_count;
//int zone_realloc_call_count;
//int zone_free_call_count;



// tests for hash table
enum { MaxItems = 16384 };


int its[MaxItems];
bool marks[MaxItems];
TMap<int, int> hash;


void checkHash (bool dump=false) {
  int count = 0;
  if (dump) printf("====== CHECK ======\n");
  for (int i = 0; i < MaxItems; ++i) {
    if (dump) {
      auto flag = hash.get(i);
      printf(" check #%d; v=%d; flag=%d\n", i, (flag ? *flag : 0), (flag ? 1 : 0));
    }
    if (its[i] >= 0) {
      ++count;
      if (!hash.has(i)) fatal("(0.0) fuuuuuuuuuuuu");
      auto vp = hash.get(i);
      if (!vp) fatal("(0.1) fuuuuuuuuuuuu");
      if (*vp != its[i]) fatal("(0.2) fuuuuuuuuuuuu");
    } else {
      if (hash.has(i)) fatal("(0.3) fuuuuuuuuuuuu");
    }
  }
  if (count != hash.count()) fatal("(0.4) fuuuuuuuuuuuu");
  if (dump) printf("------\n");
}


void testIterator () {
  int count = 0;
  for (int i = 0; i < MaxItems; ++i) marks[i] = false;
  for (auto it = hash.first(); it; ++it) {
    //writeln('key=', k, '; value=', v);
    auto k = it.getKey();
    auto v = it.getValue();
    if (marks[k]) fatal("duplicate entry in iterator");
    if (its[k] != v) fatal("invalid entry in iterator");
    marks[k] = true;
    ++count;
  }
  if (count != hash.count()) {
    printf("0: count=%d; hash.count=%d\n", count, hash.count());
    //raise Exception.Create('lost entries in iterator');
  }
  count = 0;
  for (int i = 0; i < MaxItems; ++i) if (marks[i]) ++count;
  if (count != hash.count()) {
    printf("1: count=%d; hash.count=%d\n", count, hash.count());
    fatal("lost entries in iterator");
  }
  if (hash.count() != hash.countItems()) {
    printf("OOPS: count=%d; countItems=%d\n", hash.count(), hash.countItems());
    fatal("fuck");
  }
}


int main () {
  int xcount;

  for (int i = 0; i < MaxItems; ++i) its[i] = -1;

  //Randomize();
  srand(time(nullptr));

  printf("testing: insertion\n");
  xcount = 0;
  for (int i = 0; i < MaxItems; ++i) {
    int v = rand()%MaxItems;
    //writeln('i=', i, '; v=', v, '; its[v]=', its[v]);
    if (its[v] >= 0) {
      if (!hash.has(v)) fatal("(1.0) fuuuuuuuuuuuu");
      auto vp = hash.get(v);
      if (!vp) fatal("(1.1) fuuuuuuuuuuuu");
      if (*vp != its[v]) fatal("(1.2) fuuuuuuuuuuuu");
    } else {
      its[v] = i;
      if (hash.put(v, i)) fatal("(1.3) fuuuuuuuuuuuu");
      ++xcount;
      if (xcount != hash.count()) fatal("(1.4) fuuuuuuuuuuuu");
    }
    #ifdef EXCESSIVE_CHECKS
    checkHash();
    #endif
    #ifdef EXCESSIVE_CHECKS_ITERATOR
    testIterator();
    #endif
  }
  if (xcount != hash.count()) fatal("(1.5) fuuuuuuuuuuuu");
  checkHash();
  testIterator();

  printf("testing: deletion\n");
  for (int i = 0; i < MaxItems*8; ++i) {
    int v = rand()%MaxItems;
    //writeln('trying to delete ', v, '; its[v]=', its[v]);
    bool del = hash.del(v);
    //writeln('  del=', del);
    if (del) {
      if (its[v] < 0) fatal("(2.0) fuuuuuuuuuuuu");
      --xcount;
    } else {
      if (its[v] >= 0) fatal("(2.1) fuuuuuuuuuuuu");
    }
    its[v] = -1;
    if (xcount != hash.count()) fatal("(2.2) fuuuuuuuuuuuu");
    #ifdef EXCESSIVE_COMPACT
    hash.compact();
    if (xcount != hash.count()) fatal("(2.3) fuuuuuuuuuuuu");
    #endif
    #ifdef EXCESSIVE_CHECKS
    checkHash();
    #endif
    #ifdef EXCESSIVE_CHECKS_ITERATOR
    testIterator();
    #endif
    if (hash.count() == 0) break;
  }

  printf("testing: complete\n");
  checkHash();
  testIterator();

  return 0;
}
