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
#ifndef VAVOOM_CORE_LIB_HASHFUNC
#define VAVOOM_CORE_LIB_HASHFUNC

#include "common.h"


static __attribute__((unused)) inline int digitInBase (char ch, int base=10) {
  if (base < 1 || base > 36 || ch < '0') return -1;
  if (base <= 10) return (ch < 48+base ? ch-48 : -1);
  if (ch >= '0' && ch <= '9') return ch-48;
  if (ch >= 'a' && ch <= 'z') ch -= 32; // poor man tolower()
  if (ch < 'A' || ch >= 65+(base-10)) return -1;
  return ch-65+10;
}


static __attribute__((unused)) inline char upcase1251 (char ch) {
  if ((vuint8)ch < 128) return ch-(ch >= 'a' && ch <= 'z' ? 32 : 0);
  if ((vuint8)ch >= 224 && (vuint8)ch <= 255) return (vuint8)ch-32;
  if ((vuint8)ch == 184 || (vuint8)ch == 186 || (vuint8)ch == 191) return (vuint8)ch-16;
  if ((vuint8)ch == 162 || (vuint8)ch == 179) return (vuint8)ch-1;
  return ch;
}


static __attribute__((unused)) inline char locase1251 (char ch) {
  if ((vuint8)ch < 128) return ch+(ch >= 'A' && ch <= 'Z' ? 32 : 0);
  if ((vuint8)ch >= 192 && (vuint8)ch <= 223) return (vuint8)ch+32;
  if ((vuint8)ch == 168 || (vuint8)ch == 170 || (vuint8)ch == 175) return (vuint8)ch+16;
  if ((vuint8)ch == 161 || (vuint8)ch == 178) return (vuint8)ch+1;
  return ch;
}


static __attribute__((unused)) inline vuint32 nextPOTU32 (vuint32 x) {
  vuint32 res = x;
  res |= (res>>1);
  res |= (res>>2);
  res |= (res>>4);
  res |= (res>>8);
  res |= (res>>16);
  // already pot?
  if (x != 0 && (x&(x-1)) == 0) res &= ~(res>>1); else ++res;
  return res;
}


static __attribute__((unused)) inline vuint32 hashU32 (vuint32 a) {
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


// fnv-1a: http://www.isthe.com/chongo/tech/comp/fnv/
static __attribute__((unused)) inline vuint32 fnvHashBufCI (const void *buf, size_t len) {
  vuint32 hash = 2166136261U; // fnv offset basis
  const vuint8 *s = (const vuint8 *)buf;
  while (len--) {
    hash ^= (vuint8)locase1251(*s++);
    hash *= 16777619U; // 32-bit fnv prime
  }
  return (hash ? hash : 1); // this is unlikely, but...
}


// fnv-1a: http://www.isthe.com/chongo/tech/comp/fnv/
static __attribute__((unused)) inline vuint32 fnvHashBuf (const void *buf, size_t len) {
  vuint32 hash = 2166136261U; // fnv offset basis
  if (len) {
    const vuint8 *s = (const vuint8 *)buf;
    while (len--) {
      hash ^= *s++;
      hash *= 16777619U; // 32-bit fnv prime
    }
  }
  return (hash ? hash : 1); // this is unlikely, but...
}


// fnv-1a: http://www.isthe.com/chongo/tech/comp/fnv/
static __attribute__((unused)) inline vuint32 fnvHashStr (const void *buf) {
  vuint32 hash = 2166136261U; // fnv offset basis
  if (buf) {
    const vuint8 *s = (const vuint8 *)buf;
    while (*s) {
      hash ^= *s++;
      hash *= 16777619U; // 32-bit fnv prime
    }
  }
  return (hash ? hash : 1); // this is unlikely, but...
}


// djb
static __attribute__((unused)) inline vuint32 djbHashBufCI (const void *buf, size_t len) {
  vuint32 hash = 5381;
  const vuint8 *s = (const vuint8 *)buf;
  while (len--) hash = ((hash<<5)+hash)+(vuint8)locase1251(*s++);
  return (hash ? hash : 1); // this is unlikely, but...
}


// djb
static __attribute__((unused)) inline vuint32 djbHashBuf (const void *buf, size_t len) {
  vuint32 hash = 5381;
  const vuint8 *s = (const vuint8 *)buf;
  while (len-- > 0) hash = ((hash<<5)+hash)+(*s++);
  return (hash ? hash : 1); // this is unlikely, but...
}


static __attribute__((unused)) inline vuint32 joaatHashBuf (const void *buf, size_t len, vuint32 seed=0) {
  vuint32 hash = seed;
  const vuint8 *s = (const vuint8 *)buf;
  while (len--) {
    hash += *s++;
    hash += hash<<10;
    hash ^= hash>>6;
  }
  // finalize
  hash += hash<<3;
  hash ^= hash>>11;
  hash += hash<<15;
  return hash;
}


static __attribute__((unused)) inline vuint32 joaatHashBufCI (const void *buf, size_t len, vuint32 seed=0) {
  vuint32 hash = seed;
  const vuint8 *s = (const vuint8 *)buf;
  while (len--) {
    hash += (vuint8)locase1251(*s++);
    hash += hash<<10;
    hash ^= hash>>6;
  }
  // finalize
  hash += hash<<3;
  hash ^= hash>>11;
  hash += hash<<15;
  return hash;
}


static __attribute__((unused)) inline vuint32 GetTypeHash (int n) { return hashU32((vuint32)n); }
static __attribute__((unused)) inline vuint32 GetTypeHash (vuint32 n) { return hashU32(n); }
static __attribute__((unused)) inline vuint32 GetTypeHash (vuint64 n) { return hashU32((vuint32)n)^hashU32((vuint32)(n>>32)); }
static __attribute__((unused)) inline vuint32 GetTypeHash (const void *n) { return GetTypeHash((uintptr_t)n); }

#endif
